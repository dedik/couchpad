package app.couchpad.pilot.data

import android.util.Log
import app.couchpad.pilot.data.mqtt.MqttClient
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.flow.update
import org.json.JSONObject
import java.util.UUID

/**
 * Jedno połączenie z brokerem dla całego pilota, jak MqttBus w firmware:
 *  - Z906 wprost na tematach kontrolera (ADR 0006),
 *  - telewizor wprost na tematach Glasshouse (ADR 0008),
 *  - Apple TV, klima, światła, pogoda i kanały kontraktem `multimedia_remote/v1` (ADR 0007).
 *
 * Interfejs czyta wyłącznie [state], [broker] i [prefs]. Polecenia nie czekają na sieć:
 * oznaczają kontrolkę jako oczekującą ([Pending]), a zaznaczenie zmienia dopiero stan
 * odebrany z brokera.
 */
class PilotRepository(private val store: SettingsStore) {

    private val _state = MutableStateFlow(PilotState())
    val state: StateFlow<PilotState> = _state.asStateFlow()

    private val _broker = MutableStateFlow(store.loadBroker())
    val broker: StateFlow<BrokerSettings> = _broker.asStateFlow()

    private val _prefs = MutableStateFlow(store.loadPreferences())
    val prefs: StateFlow<Preferences> = _prefs.asStateFlow()

    // Pisane z wątku UI, czytane z wątku klienta MQTT.
    @Volatile private var client: MqttClient? = null
    @Volatile private var settings = _broker.value
    private val tv = TvAggregator()

    private val z906Base get() = settings.z906Base
    private val v1Base get() = settings.v1Base

    // --------------------------------------------------------------- cykl życia

    fun start() = connect(settings)

    fun stop() {
        client?.stop()
        client = null
    }

    /** Aplikacja wraca na wierzch: wymuszamy świeży stan (ADR 0005). */
    fun wake() {
        _state.update { it.copy(link = it.link.copy(freshSince = 0)) }
        val c = client
        when {
            c == null -> connect(settings)
            !c.connected -> c.reconnectNow()
            else -> requestSync()
        }
    }

    /** „Uruchom ponownie”: nowe połączenie i świeży stan, ustawienia zostają. */
    fun restart() = connect(settings)

    fun saveBroker(newSettings: BrokerSettings) {
        store.saveBroker(newSettings)
        _broker.value = newSettings
        connect(newSettings)
    }

    fun updatePreferences(change: (Preferences) -> Preferences) {
        val next = change(_prefs.value)
        store.savePreferences(next)
        _prefs.value = next
    }

    /** Ustawienia fabryczne: zapomina broker i preferencje, łączy się od nowa (bez danych). */
    fun factoryReset() {
        store.clearAll()
        _prefs.value = store.loadPreferences()
        val broker = store.loadBroker()
        _broker.value = broker
        connect(broker)
    }

    private fun connect(s: BrokerSettings) {
        stop()
        settings = s
        tv.reset()
        // Stan urządzeń z poprzedniego połączenia nie jest już aktualny; pogoda i kanały zostają.
        _state.update { PilotState(link = it.link, weather = it.weather, channels = it.channels) }
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
            willTopic = if (s.remoteId.isNotBlank()) "${s.v1Base}/availability" else null,
        )
        _state.update { it.copy(link = LinkState(LinkPhase.Connecting)) }
        // Najpierw przypisanie: onConnected z wątku klienta czyta `client`.
        val c = MqttClient(config, listener)
        client = c
        c.start()
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
                c.subscribe("${s.z906Base}/state/#")
                c.subscribe("${s.z906Base}/availability")
            }
            subscribeTv(c, s)
            if (s.remoteId.isNotBlank()) {
                for (leaf in listOf("state", "channels", "weather")) c.subscribe("${s.v1Base}/$leaf")
                c.publish("${s.v1Base}/availability", "online", 1, retain = true)
                command("remote.sync.request")
                command("tv.channels.request")
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
            discover(s, topic)?.let { found ->
                Log.i(TAG, "wykryto identyfikatory u brokera")
                saveBroker(found)
                return
            }
            // Wszystkie prefiksy z jednej migawki ustawień — zmiana brokera w trakcie nie miesza.
            val z906 = "${s.z906Base}/"
            val tvPrefix = "${s.tvPrefix}/"
            val v1 = "${s.v1Base}/"
            when {
                s.z906Id.isNotBlank() && topic.startsWith(z906) -> onZ906(topic.removePrefix(z906), text)
                s.tvPrefix.isNotBlank() && topic.startsWith(tvPrefix) -> onTv(topic.removePrefix(tvPrefix), text)
                s.remoteId.isNotBlank() && topic.startsWith(v1) -> onContract(topic.removePrefix(v1), text)
            }
        }
    }

    private fun subscribeTv(c: MqttClient, s: BrokerSettings = settings) {
        val prefix = s.tvPrefix
        if (prefix.isBlank()) return
        for (leaf in TvAggregator.leaves) c.subscribe("$prefix/$leaf")
    }

    /** Ponowna subskrypcja oddaje komplet retained — to żądanie odświeżenia Z906 i TV. */
    private fun requestSync() {
        val c = client ?: return
        if (!c.connected) return
        val s = settings
        if (s.z906Id.isNotBlank()) c.subscribe("${s.z906Base}/state/#")
        subscribeTv(c, s)
        command("remote.sync.request")
    }

    // --------------------------------------------------------------- stan przychodzący

    private fun onZ906(leaf: String, value: String) {
        val now = System.currentTimeMillis()
        _state.update { st ->
            val z = st.z906.withTopic(leaf, value) ?: return@update st
            st.copy(
                z906 = z.copy(updatedAt = now),
                pending = st.pending.withoutPrefixes(Pending.Z906.prefix),
                link = st.link.markFresh(now),
            )
        }
    }

    private fun onTv(leaf: String, value: String) {
        val next = tv.apply(leaf, value, System.currentTimeMillis()) ?: return
        _state.update { it.copy(tv = next, pending = it.pending.withoutPrefixes(Pending.Tv.prefix)) }
    }

    private fun onContract(leaf: String, text: String) {
        val now = System.currentTimeMillis()
        when (leaf) {
            "state" -> Contract.parseSnapshot(text, now)?.let { onSnapshot(it, now) }
            "channels" -> Contract.parseChannels(text)?.let { list -> _state.update { it.copy(channels = list) } }
            "weather" -> Contract.parseWeather(text, now)?.let { w -> _state.update { it.copy(weather = w) } }
        }
    }

    private fun onSnapshot(home: HomeSnapshot, now: Long) {
        _state.update { st ->
            // Rewizja nie większa od ostatniej to powtórka (np. retained) — ignorujemy,
            // chyba że czekamy na pierwszy stan po wybudzeniu.
            if (home.revision >= 0 && home.revision <= st.home.revision && st.home.known && st.link.fresh) {
                return@update st
            }
            st.copy(
                home = home,
                pending = st.pending.withoutPrefixes(Pending.AppleTv.prefix, Pending.Climate.prefix, Pending.Light.prefix),
                link = st.link.markFresh(now),
            )
        }
    }

    // --------------------------------------------------------------- polecenia: Z906

    private fun z906(leaf: String, value: String, pending: String) {
        if (settings.z906Id.isBlank()) return
        if (client?.publish("$z906Base/command/$leaf", value, 1) == true) markPending(pending)
    }

    private fun onOff(on: Boolean) = if (on) "ON" else "OFF"

    fun z906Power(on: Boolean) = z906("power", onOff(on), Pending.Z906.power)
    fun z906Mute(muted: Boolean) = z906("mute", onOff(muted), Pending.Z906.mute)
    fun z906Volume(percent: Int) =
        z906("volume", percent.coerceIn(0, _prefs.value.volumeLimit).toString(), Pending.Z906.volume)
    fun z906Level(channel: Z906Channel, percent: Int) =
        z906(channel.topic, percent.coerceIn(0, 100).toString(), Pending.Z906.level(channel))
    fun z906Source(source: String) = z906("source", source, Pending.Z906.source)
    fun z906Effect(effect: String) = z906("effect", effect, Pending.Z906.effect)

    /** −/+ głośności o [levels] poziomów wzmacniacza; bez znanej głośności nic nie robi. */
    fun z906VolumeStep(levels: Int) {
        val v = _state.value.z906.volume ?: return
        z906Volume(Z906.stepVolume(v, levels, _prefs.value.volumeLimit))
    }

    // --------------------------------------------------------------- polecenia: telewizor

    private fun tvCommand(name: String, value: String, qos: Int, pending: String? = null) {
        val prefix = settings.tvPrefix
        if (prefix.isBlank()) return
        if (client?.publish("$prefix/command/$name", value, qos) == true && pending != null) markPending(pending)
    }

    fun tvKey(key: String) = tvCommand("rcu", key, 0)
    fun tvChannelStep(up: Boolean) = tvKey(if (up) "channelup" else "channeldown")
    fun tvSource(id: String) = tvCommand("input", id, 1, Pending.Tv.source)
    fun tvPictureMode(id: String) = tvCommand("picture_mode", id, 1, Pending.Tv.picture)
    fun tvChannel(name: String, id: String? = null) {
        val args = JSONObject().put("name", name)
        if (id != null) args.put("id", id)
        command("tv.channel.open", args)
    }

    // --------------------------------------------------------------- polecenia: kontrakt v1

    /** Polecenie kontraktu; przy [pending] kontrolka czeka na potwierdzenie stanem. */
    private fun command(action: String, args: JSONObject = JSONObject(), pending: String? = null, qos: Int = 1) {
        if (settings.remoteId.isBlank()) return
        val body = Contract.command(UUID.randomUUID().toString(), action, args)
        if (client?.publish("$v1Base/command", body, qos) == true && pending != null) markPending(pending)
    }

    fun appleTvNavigate(key: String, action: String = "tap") {
        val args = JSONObject().put("key", key)
        if (action != "tap") args.put("action", action)
        command("appletv.navigate", args, qos = 0)
    }
    fun appleTvPower(on: Boolean) = command("appletv.power.set", JSONObject().put("on", on), Pending.AppleTv.power)
    fun appleTvLaunch(appId: String) = command("appletv.app.launch", JSONObject().put("app_id", appId), Pending.AppleTv.app)
    fun appleTvText(text: String) = command("appletv.keyboard.set", JSONObject().put("text", text))

    fun climatePower(on: Boolean) = command("climate.power.set", JSONObject().put("on", on), Pending.Climate.power)
    fun climateTarget(t: Double) = command("climate.target.set", JSONObject().put("temperature", t), Pending.Climate.target)
    fun climateMode(mode: String) = command("climate.mode.set", modeArgs(mode), Pending.Climate.mode)
    fun climateFan(mode: String) = command("climate.fan.set", modeArgs(mode), Pending.Climate.fan)
    fun climateSwing(mode: String) = command("climate.swing.set", modeArgs(mode), Pending.Climate.swing)
    fun climateSwingHorizontal(mode: String) =
        command("climate.swing_horizontal.set", modeArgs(mode), Pending.Climate.swingHorizontal)

    private fun modeArgs(mode: String) = JSONObject().put("mode", mode)

    fun lightPower(id: String, on: Boolean) = light("light.power.set", id) { put("on", on) }
    fun lightBrightness(id: String, percent: Int) =
        light("light.brightness.set", id) { put("percent", percent.coerceIn(1, 100)) }
    fun lightKelvin(id: String, kelvin: Int) = light("light.color_temp.set", id) { put("kelvin", kelvin) }
    fun lightEffect(id: String, effect: String) = light("light.effect.set", id) { put("effect", effect) }

    private fun light(action: String, id: String, args: JSONObject.() -> Unit) =
        command(action, JSONObject().put("id", id).apply(args), Pending.Light.of(id))

    // --------------------------------------------------------------- oczekiwanie na stan

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

        /**
         * Ustawienia uzupełnione o identyfikator wykryty w temacie albo null: kontroler Z906
         * publikuje `z906/z906_<mac>/availability`, bridge HA — `multimedia_remote/v1/<id>/state`.
         */
        internal fun discover(s: BrokerSettings, topic: String): BrokerSettings? {
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
    }
}
