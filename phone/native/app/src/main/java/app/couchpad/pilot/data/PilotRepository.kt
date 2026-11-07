package app.couchpad.pilot.data

import android.util.Log
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import org.json.JSONArray
import org.json.JSONObject
import java.util.UUID

/**
 * Jedno połączenie z brokerem dla całego pilota, jak MqttBus w firmware:
 *  - Z906 wprost na tematach kontrolera (ADR 0006),
 *  - telewizor wprost na tematach Glasshouse (ADR 0008),
 *  - Apple TV, klima, światła, pogoda i kanały kontraktem `multimedia_remote/v1` (ADR 0007).
 *
 * Interfejs czyta wyłącznie [state]. Polecenia nie czekają na sieć: zaznaczenie zmienia
 * dopiero stan odebrany z brokera.
 */
class PilotRepository(private val store: SettingsStore) {

    private val _state = MutableStateFlow(PilotState())
    val state: StateFlow<PilotState> = _state.asStateFlow()

    private val _broker = MutableStateFlow(store.loadBroker())
    val broker: StateFlow<BrokerSettings> = _broker.asStateFlow()

    private val _prefs = MutableStateFlow(store.loadPreferences())
    val prefs: StateFlow<Preferences> = _prefs.asStateFlow()

    private var client: MqttClient? = null
    private var settings = _broker.value

    // Telewizor: telemetria co 10 s plus tematy natychmiastowe (ADR 0008).
    private var tvInputs: List<Option> = emptyList()
    private var tvModes: List<Option> = emptyList()
    private var tvStatus = ""
    private var tvApp = ""
    private var tvPicture = ""
    private var tvSystemOn: Boolean? = null
    private var tvLiveApp = false
    private var tvLivePicture = false

    // --------------------------------------------------------------- cykl życia

    fun start() {
        connect(settings)
    }

    fun stop() {
        client?.stop()
        client = null
    }

    /** Aplikacja wraca na wierzch: wymuszamy świeży stan (ADR 0005). */
    fun wake() {
        _state.update { it.copy(link = it.link.copy(freshSince = 0)) }
        val c = client
        if (c == null || !c.connected) {
            c?.reconnectNow() ?: connect(settings)
            return
        }
        requestSync()
    }

    fun saveBroker(newSettings: BrokerSettings) {
        store.saveBroker(newSettings)
        _broker.value = newSettings
        connect(newSettings)
    }

    fun savePreferences(p: Preferences) {
        store.savePreferences(p)
        _prefs.value = p
    }

    fun updatePreferences(change: (Preferences) -> Preferences) = savePreferences(change(_prefs.value))

    /** Ustawienia fabryczne: zapomina broker i preferencje, łączy się od nowa (bez danych). */
    fun factoryReset() {
        store.clearAll()
        _prefs.value = store.loadPreferences()
        val broker = store.loadBroker()
        _broker.value = broker
        connect(broker)
    }

    /** „Uruchom ponownie”: nowe połączenie i świeży stan, ustawienia zostają. */
    fun restart() {
        connect(settings)
    }

    private fun connect(s: BrokerSettings) {
        client?.stop()
        client = null
        settings = s
        resetDeviceState()
        if (!s.complete) {
            _state.update { it.copy(link = LinkState(LinkPhase.Error, "Brak ustawień brokera")) }
            return
        }
        val id = s.remoteId.ifBlank { s.z906Id.ifBlank { "telefon" } }
        val config = MqttClient.Config(
            host = s.host.trim(),
            port = s.port,
            clientId = "pilot-$id-android",
            username = s.username,
            password = s.password,
            willTopic = if (s.remoteId.isNotBlank()) "$V1/${s.remoteId}/availability" else null,
        )
        _state.update { it.copy(link = LinkState(LinkPhase.Connecting)) }
        val c = MqttClient(config, listener)
        client = c
        c.start()
    }

    private fun resetDeviceState() {
        tvInputs = emptyList(); tvModes = emptyList(); tvStatus = ""; tvApp = ""; tvPicture = ""
        tvSystemOn = null; tvLiveApp = false; tvLivePicture = false
        _state.update {
            PilotState(link = it.link, weather = it.weather, channels = it.channels)
        }
    }

    private val listener = object : MqttClient.Listener {
        override fun onConnected() {
            val c = client ?: return
            val s = settings
            // Puste identyfikatory wykrywamy u brokera z wiadomości retained:
            // kontroler Z906 publikuje dostępność, bridge HA — snapshot.
            if (s.z906Id.isBlank()) c.subscribe("z906/+/availability")
            if (s.remoteId.isBlank()) c.subscribe("$V1/+/state")
            if (s.z906Id.isNotBlank()) {
                c.subscribe("z906/${s.z906Id}/state/#")
                c.subscribe("z906/${s.z906Id}/availability")
            }
            if (s.tvPrefix.isNotBlank()) subscribeTv(c, s.tvPrefix)
            if (s.remoteId.isNotBlank()) {
                val base = "$V1/${s.remoteId}"
                c.subscribe("$base/state")
                c.subscribe("$base/channels")
                c.subscribe("$base/weather")
                c.publish("$base/availability", "online", 1, retain = true)
                command("remote.sync.request", JSONObject())
                command("tv.channels.request", JSONObject())
            }
            _state.update { it.copy(link = it.link.copy(phase = LinkPhase.Connected, error = null)) }
        }

        override fun onDisconnected(failure: MqttClient.Failure) {
            val message = when (failure) {
                MqttClient.Failure.NotAuthorized -> "Broker odrzucił login lub hasło"
                is MqttClient.Failure.Rejected -> "Broker odrzucił połączenie (${failure.code})"
                MqttClient.Failure.Unreachable -> "Nie mogę połączyć się z brokerem"
                MqttClient.Failure.Dropped -> "Połączenie z brokerem zerwane"
            }
            _state.update { it.copy(link = LinkState(LinkPhase.Error, message)) }
        }

        override fun onMessage(topic: String, payload: ByteArray, retained: Boolean) {
            val text = String(payload, Charsets.UTF_8)
            val s = settings
            discover(topic)?.let { found ->
                Log.i(TAG, "wykryto identyfikatory u brokera")
                saveBroker(found)
                return
            }
            when {
                s.z906Id.isNotBlank() && topic.startsWith("z906/${s.z906Id}/") ->
                    applyZ906(topic.removePrefix("z906/${s.z906Id}/"), text)
                s.tvPrefix.isNotBlank() && topic.startsWith("${s.tvPrefix}/") ->
                    applyTv(topic.removePrefix("${s.tvPrefix}/"), text)
                s.remoteId.isNotBlank() && topic.startsWith("$V1/${s.remoteId}/") ->
                    applyContract(topic.removePrefix("$V1/${s.remoteId}/"), text)
            }
        }
    }

    /** Ustawienia uzupełnione o identyfikator wykryty w temacie albo null. */
    private fun discover(topic: String): BrokerSettings? {
        val s = settings
        val parts = topic.split('/')
        if (s.z906Id.isBlank() && parts.size == 3 && parts[0] == "z906" && parts[2] == "availability" &&
            parts[1].startsWith("z906_")
        ) {
            return s.copy(z906Id = parts[1])
        }
        if (s.remoteId.isBlank() && parts.size == 4 && "${parts[0]}/${parts[1]}" == V1 && parts[3] == "state") {
            return s.copy(remoteId = parts[2])
        }
        return null
    }

    private fun subscribeTv(c: MqttClient, prefix: String) {
        for (leaf in listOf("status", "telemetry", "state/application/app", "state/picture/pictureMode",
            "state/power/systemOn")) {
            c.subscribe("$prefix/$leaf")
        }
    }

    /** Ponowna subskrypcja oddaje komplet retained — to żądanie odświeżenia Z906 i TV. */
    fun requestSync() {
        val c = client ?: return
        if (!c.connected) return
        val s = settings
        if (s.z906Id.isNotBlank()) c.subscribe("z906/${s.z906Id}/state/#")
        if (s.tvPrefix.isNotBlank()) subscribeTv(c, s.tvPrefix)
        if (s.remoteId.isNotBlank()) command("remote.sync.request", JSONObject())
    }

    // --------------------------------------------------------------- Z906

    private fun applyZ906(leaf: String, value: String) {
        val now = System.currentTimeMillis()
        _state.update { st ->
            var z = st.z906
            when (leaf) {
                "availability" -> z = z.copy(available = value == "online")
                "state/power" -> z = z.copy(power = value.equals("ON", true))
                "state/mute" -> z = z.copy(muted = value.equals("ON", true))
                "state/volume" -> z = z.copy(volume = value.toIntOrNull())
                "state/centre" -> z = z.copy(center = value.toIntOrNull())
                "state/subwoofer" -> z = z.copy(subwoofer = value.toIntOrNull())
                "state/rear" -> z = z.copy(rear = value.toIntOrNull())
                "state/source" -> z = z.copy(source = value)
                "state/effect" -> z = z.copy(effect = value)
                "state/decode" -> z = z.copy(decode = value.equals("ON", true))
                else -> return@update st
            }
            if (leaf != "availability") z = z.copy(available = true)
            st.copy(
                z906 = z.copy(updatedAt = now),
                pending = st.pending.filterKeys { !it.startsWith("z906.") },
                link = st.link.copy(freshSince = if (st.link.freshSince == 0L) now else st.link.freshSince),
            )
        }
    }

    private fun z906Command(leaf: String, value: String, key: String) {
        val id = settings.z906Id
        if (id.isBlank()) return
        if (client?.publish("z906/$id/command/$leaf", value, 1) == true) markPending(key)
    }

    fun z906Power(on: Boolean) = z906Command("power", if (on) "ON" else "OFF", "z906.power")
    fun z906Mute(muted: Boolean) = z906Command("mute", if (muted) "ON" else "OFF", "z906.mute")
    fun z906Volume(percent: Int) {
        val limited = percent.coerceIn(0, _prefs.value.volumeLimit)
        z906Command("volume", limited.toString(), "z906.volume")
    }
    fun z906Level(channel: String, percent: Int) {
        val leaf = if (channel == "center") "centre" else channel
        z906Command(leaf, percent.coerceIn(0, 100).toString(), "z906.$channel")
    }
    fun z906Source(source: String) = z906Command("source", source, "z906.source")
    fun z906Effect(effect: String) = z906Command("effect", effect, "z906.effect")

    // --------------------------------------------------------------- telewizor

    private fun applyTv(leaf: String, value: String) {
        when (leaf) {
            "telemetry" -> parseTelemetry(value)
            "status" -> tvStatus = value.trim()
            "state/application/app" -> { tvApp = value.trim(); tvLiveApp = true }
            "state/picture/pictureMode" -> { tvPicture = value.trim(); tvLivePicture = true }
            "state/power/systemOn" -> tvSystemOn = value.trim() == "true"
            else -> return
        }
        val online = tvStatus == "online"
        val tv = TvState(
            available = online || tvStatus == "off",
            status = tvStatus,
            power = online && tvSystemOn != false,
            sources = tvInputs,
            source = tvApp,
            pictureModes = tvModes,
            pictureMode = tvPicture,
            updatedAt = System.currentTimeMillis(),
        )
        _state.update { st ->
            st.copy(tv = tv, pending = st.pending.filterKeys { !it.startsWith("tv.") })
        }
    }

    private fun parseTelemetry(text: String) {
        val root = runCatching { JSONObject(text) }.getOrNull() ?: return
        // Wejścia: Live TV na początku, potem HDMI w kolejności numerów. Nazwy z ustawień TV.
        val inputs = mutableListOf(Option("livetv", "TV"))
        root.optJSONObject("inputs")?.let { obj ->
            obj.keys().asSequence().sorted().forEach { key ->
                inputs += Option(key, obj.optString(key, key))
            }
        }
        tvInputs = inputs
        root.optJSONObject("picture")?.let { picture ->
            val modes = picture.optJSONArray("modes")
            if (modes != null && modes.length() > 0) {
                tvModes = (0 until modes.length()).mapNotNull { i ->
                    val m = modes.optJSONObject(i) ?: return@mapNotNull null
                    val value = m.optString("value")
                    if (value.isEmpty()) null else Option(value, m.optString("label").ifEmpty { value })
                }
            }
            if (!tvLivePicture) tvPicture = picture.optString("mode_raw")
        }
        if (!tvLiveApp) {
            tvApp = root.optString("input_source").ifEmpty { root.optString("app") }
        }
        if (tvSystemOn == null) {
            root.optJSONObject("powerState")?.let { p ->
                if (p.has("systemOn")) tvSystemOn = p.optBoolean("systemOn")
            }
        }
    }

    private fun tvCommand(name: String, value: String, qos: Int, key: String?) {
        val prefix = settings.tvPrefix
        if (prefix.isBlank()) return
        if (client?.publish("$prefix/command/$name", value, qos) == true && key != null) markPending(key)
    }

    fun tvKey(key: String) = tvCommand("rcu", key, 0, null)
    fun tvSource(id: String) = tvCommand("input", id, 1, "tv.source")
    fun tvPictureMode(id: String) = tvCommand("picture_mode", id, 1, "tv.picture")
    fun tvChannelStep(up: Boolean) = tvCommand("rcu", if (up) "channelup" else "channeldown", 0, null)
    fun tvChannel(name: String, id: String? = null) {
        val args = JSONObject().put("name", name)
        if (id != null) args.put("id", id)
        command("tv.channel.open", args)
    }

    // --------------------------------------------------------------- kontrakt v1

    private fun applyContract(leaf: String, text: String) {
        when (leaf) {
            "state" -> parseSnapshot(text)
            "channels" -> parseChannels(text)
            "weather" -> parseWeather(text)
        }
    }

    private fun parseSnapshot(text: String) {
        val root = runCatching { JSONObject(text) }.getOrNull() ?: return
        val revision = root.optLong("revision", -1)
        val now = System.currentTimeMillis()
        val atv = root.optJSONObject("appletv")?.let { a ->
            AppleTvState(
                available = a.optBoolean("available"),
                power = a.optString("power") == "on",
                appId = a.optString("app_id"),
                apps = a.optJSONArray("apps").objects().map { AppleTvApp(it.optString("id"), it.optString("label")) },
                keyboardActive = a.optBoolean("keyboard_active"),
            )
        } ?: AppleTvState()
        val climate = root.optJSONObject("climate")?.let { c ->
            ClimateState(
                available = c.optBoolean("available"),
                hvacMode = c.optString("hvac_mode", "off"),
                hvacModes = c.optJSONArray("hvac_modes").strings(),
                target = c.optDoubleOrNull("target"),
                current = c.optDoubleOrNull("current"),
                min = c.optDouble("min", 16.0),
                max = c.optDouble("max", 31.0),
                step = c.optDouble("step", 0.5),
                fanMode = c.optString("fan_mode"),
                fanModes = c.optJSONArray("fan_modes").strings(),
                swingMode = c.optString("swing_mode"),
                swingModes = c.optJSONArray("swing_modes").strings(),
                swingHorizontalMode = c.optString("swing_horizontal_mode"),
                swingHorizontalModes = c.optJSONArray("swing_horizontal_modes").strings(),
            )
        } ?: ClimateState()
        val lights = root.optJSONArray("lights").objects().map { l ->
            Light(
                room = l.optString("room"),
                id = l.optString("id"),
                label = l.optString("label"),
                available = l.optBoolean("available"),
                on = l.optBoolean("on"),
                dimmable = l.optBoolean("dimmable", true),
                brightness = l.optInt("brightness"),
                kelvin = l.optInt("kelvin"),
                minKelvin = l.optInt("min_kelvin"),
                maxKelvin = l.optInt("max_kelvin"),
                effect = l.optString("effect"),
                effects = l.optJSONArray("effects").strings(),
                members = l.optJSONArray("members").objects().map { m ->
                    LightMember(
                        id = m.optString("id"), label = m.optString("label"),
                        available = m.optBoolean("available"), on = m.optBoolean("on"),
                        brightness = m.optInt("brightness"), kelvin = m.optInt("kelvin"),
                    )
                },
            )
        }
        _state.update { st ->
            // Rewizja nie większa od ostatniej to powtórka (np. retained) — ignorujemy,
            // chyba że czekamy na pierwszy stan po wybudzeniu.
            if (revision >= 0 && revision <= st.home.revision && st.home.known && st.link.freshSince != 0L) {
                return@update st
            }
            st.copy(
                home = HomeSnapshot(revision, atv, climate, lights, now),
                pending = st.pending.filterKeys {
                    !(it.startsWith("appletv.") || it.startsWith("climate.") || it.startsWith("light."))
                },
                link = st.link.copy(freshSince = if (st.link.freshSince == 0L) now else st.link.freshSince),
            )
        }
    }

    private fun parseChannels(text: String) {
        val root = runCatching { JSONObject(text) }.getOrNull() ?: return
        val list = root.optJSONArray("channels").objects().map {
            Channel(it.optString("number"), it.optString("name"), it.optString("id"))
        }
        _state.update { it.copy(channels = list) }
    }

    private fun parseWeather(text: String) {
        val root = runCatching { JSONObject(text) }.getOrNull() ?: return
        val indoor = root.optJSONObject("indoor")
        _state.update {
            it.copy(
                weather = Weather(
                    condition = root.optString("condition"),
                    temperature = root.optDoubleOrNull("temperature"),
                    night = root.optBoolean("night"),
                    indoorTemperature = indoor?.optDoubleOrNull("temperature"),
                    indoorHumidity = indoor?.let { i -> if (i.has("humidity")) i.optInt("humidity") else null },
                    receivedAt = System.currentTimeMillis(),
                )
            )
        }
    }

    private fun command(action: String, args: JSONObject, qos: Int = 1): Boolean {
        val id = settings.remoteId
        if (id.isBlank()) return false
        val body = JSONObject()
            .put("id", UUID.randomUUID().toString())
            .put("action", action)
            .put("args", args)
        return client?.publish("$V1/$id/command", body.toString(), qos) == true
    }

    fun appleTvNavigate(key: String, action: String = "tap") {
        val args = JSONObject().put("key", key)
        if (action != "tap") args.put("action", action)
        command("appletv.navigate", args, qos = 0)
    }
    fun appleTvPower(on: Boolean) {
        if (command("appletv.power.set", JSONObject().put("on", on))) markPending("appletv.power")
    }
    fun appleTvLaunch(appId: String) {
        if (command("appletv.app.launch", JSONObject().put("app_id", appId))) markPending("appletv.app")
    }
    fun appleTvText(text: String) = command("appletv.keyboard.set", JSONObject().put("text", text))

    fun climatePower(on: Boolean) {
        if (command("climate.power.set", JSONObject().put("on", on))) markPending("climate.power")
    }
    fun climateTarget(t: Double) {
        if (command("climate.target.set", JSONObject().put("temperature", t))) markPending("climate.target")
    }
    fun climateMode(mode: String) {
        if (command("climate.mode.set", JSONObject().put("mode", mode))) markPending("climate.mode")
    }
    fun climateFan(mode: String) {
        if (command("climate.fan.set", JSONObject().put("mode", mode))) markPending("climate.fan")
    }
    fun climateSwing(mode: String) {
        if (command("climate.swing.set", JSONObject().put("mode", mode))) markPending("climate.swing")
    }
    fun climateSwingHorizontal(mode: String) {
        if (command("climate.swing_horizontal.set", JSONObject().put("mode", mode))) markPending("climate.swing_h")
    }

    fun lightPower(id: String, on: Boolean) {
        if (command("light.power.set", JSONObject().put("id", id).put("on", on))) markPending("light.$id")
    }
    fun lightBrightness(id: String, percent: Int) {
        if (command("light.brightness.set", JSONObject().put("id", id).put("percent", percent.coerceIn(1, 100)))) {
            markPending("light.$id")
        }
    }
    fun lightKelvin(id: String, kelvin: Int) {
        if (command("light.color_temp.set", JSONObject().put("id", id).put("kelvin", kelvin))) markPending("light.$id")
    }
    fun lightEffect(id: String, effect: String) {
        if (command("light.effect.set", JSONObject().put("id", id).put("effect", effect))) markPending("light.$id")
    }

    private fun markPending(key: String) {
        val now = System.currentTimeMillis()
        _state.update { it.copy(pending = it.pending + (key to now)) }
    }

    /** Usuwa oczekiwania starsze niż limit — UI pokaże wtedy brak potwierdzenia. */
    fun expirePending(limitMillis: Long = 3000) {
        val now = System.currentTimeMillis()
        _state.update { st ->
            val kept = st.pending.filterValues { now - it < limitMillis }
            if (kept.size == st.pending.size) st else st.copy(pending = kept)
        }
    }

    companion object {
        const val V1 = "multimedia_remote/v1"
        private const val TAG = "repo"
    }
}

private fun JSONArray?.objects(): List<JSONObject> =
    if (this == null) emptyList() else (0 until length()).mapNotNull { optJSONObject(it) }

private fun JSONArray?.strings(): List<String> =
    if (this == null) emptyList() else (0 until length()).mapNotNull { optString(it, null) }

private fun JSONObject.optDoubleOrNull(name: String): Double? =
    if (!has(name) || isNull(name)) null else optDouble(name).takeIf { !it.isNaN() }
