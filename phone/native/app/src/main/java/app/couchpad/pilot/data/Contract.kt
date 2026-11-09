package app.couchpad.pilot.data

import org.json.JSONObject

/**
 * Wiadomości kontraktu `multimedia_remote/v1/<remote_id>/…` od bridge'u HA (ADR 0007):
 * snapshot `state`, lista `channels` i `weather`. Czyste funkcje — bez stanu i sieci.
 */
internal object Contract {

    /** Snapshot z rewizją; [HomeSnapshot.receivedAt] ustawia odbiorca. */
    fun parseSnapshot(text: String, receivedAt: Long): HomeSnapshot? {
        val root = parseObject(text) ?: return null
        return HomeSnapshot(
            revision = root.optLong("revision", -1),
            appleTv = root.optJSONObject("appletv")?.let(::appleTv) ?: AppleTvState(),
            climate = root.optJSONObject("climate")?.let(::climate) ?: ClimateState(),
            lights = root.optJSONArray("lights").objects().map(::light),
            // Najwyżej cztery głowice, jak w firmware; brak sekcji chowa karty grzejników.
            radiators = root.optJSONArray("radiators").objects()
                .filter { it.optString("room").isNotEmpty() && it.optString("id").isNotEmpty() }
                .take(4)
                .map(::radiator),
            receivedAt = receivedAt,
        )
    }

    fun parseChannels(text: String): List<Channel>? {
        val root = parseObject(text) ?: return null
        return root.optJSONArray("channels").objects().map {
            Channel(it.optString("number"), it.optString("name"), it.optString("id"))
        }
    }

    fun parseWeather(text: String, receivedAt: Long): Weather? {
        val root = parseObject(text) ?: return null
        val indoor = root.optJSONObject("indoor")
        return Weather(
            condition = root.optString("condition"),
            temperature = root.optDoubleOrNull("temperature"),
            night = root.optBoolean("night"),
            indoorTemperature = indoor?.optDoubleOrNull("temperature"),
            indoorHumidity = indoor?.optIntOrNull("humidity"),
            rooms = indoor?.optJSONArray("rooms").objects()
                .filter { it.optString("room").isNotEmpty() }
                .take(6)
                .map { RoomReading(it.optString("room"), it.optDoubleOrNull("temperature"), it.optIntOrNull("humidity")) },
            receivedAt = receivedAt,
        )
    }

    private fun appleTv(a: JSONObject) = AppleTvState(
        available = a.optBoolean("available"),
        power = a.optString("power") == "on",
        appId = a.optString("app_id"),
        apps = a.optJSONArray("apps").objects().map { AppleTvApp(it.optString("id"), it.optString("label")) },
        keyboardActive = a.optBoolean("keyboard_active"),
    )

    private fun climate(c: JSONObject) = ClimateState(
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

    private fun light(l: JSONObject) = Light(
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

    private fun radiator(r: JSONObject) = Radiator(
        room = r.optString("room"),
        id = r.optString("id"),
        available = r.optBoolean("available"),
        mode = r.optString("mode", "off"),
        modes = r.optJSONArray("modes").strings(),
        target = r.optDoubleOrNull("target"),
        min = r.optDouble("min", 5.0),
        max = r.optDouble("max", 30.0),
        step = r.optDouble("step", 0.5).takeIf { it > 0 } ?: 0.5,
        heating = r.optBoolean("heating"),
        battery = r.optIntOrNull("battery"),
        windowOpen = if (r.has("window_open") && !r.isNull("window_open")) r.optBoolean("window_open") else null,
        next = r.optJSONObject("next")?.let { n ->
            val at = n.optString("at")
            n.optDoubleOrNull("target")?.takeIf { at.isNotEmpty() }?.let { RadiatorNext(at, it) }
        },
    )

    /** Treść polecenia `…/command`: identyfikator, akcja i argumenty. */
    fun command(id: String, action: String, args: JSONObject): String =
        JSONObject().put("id", id).put("action", action).put("args", args).toString()
}
