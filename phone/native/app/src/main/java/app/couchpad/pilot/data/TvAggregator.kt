package app.couchpad.pilot.data

/**
 * Stan telewizora z tematów Glasshouse (ADR 0008): telemetria co 10 s z listą wejść i
 * trybów obrazu oraz tematy natychmiastowe (aplikacja, tryb obrazu, zasilanie). Wartość
 * z tematu natychmiastowego wygrywa z telemetrią, która może być do 10 s starsza.
 */
internal class TvAggregator {
    private var inputs: List<Option> = emptyList()
    private var modes: List<Option> = emptyList()
    private var status = ""
    private var app = ""
    private var picture = ""
    private var systemOn: Boolean? = null
    private var liveApp = false
    private var livePicture = false

    fun reset() {
        inputs = emptyList(); modes = emptyList(); status = ""; app = ""; picture = ""
        systemOn = null; liveApp = false; livePicture = false
    }

    /** Nowy stan po wiadomości z liścia [leaf] albo null, gdy liść nie jest znany. */
    fun apply(leaf: String, value: String, now: Long): TvState? {
        when (leaf) {
            "telemetry" -> telemetry(value)
            "status" -> status = value.trim()
            "state/application/app" -> { app = value.trim(); liveApp = true }
            "state/picture/pictureMode" -> { picture = value.trim(); livePicture = true }
            "state/power/systemOn" -> systemOn = value.trim() == "true"
            else -> return null
        }
        val online = status == "online"
        return TvState(
            available = online || status == "off",
            status = status,
            power = online && systemOn != false,
            sources = inputs,
            source = app,
            pictureModes = modes,
            pictureMode = picture,
            updatedAt = now,
        )
    }

    private fun telemetry(text: String) {
        val root = parseObject(text) ?: return
        // Wejścia: Live TV na początku, potem HDMI w kolejności numerów. Nazwy z ustawień TV.
        val list = mutableListOf(Option("livetv", "TV"))
        root.optJSONObject("inputs")?.let { obj ->
            obj.keys().asSequence().sorted().forEach { key -> list += Option(key, obj.optString(key, key)) }
        }
        inputs = list
        root.optJSONObject("picture")?.let { p ->
            val parsed = p.optJSONArray("modes").objects().mapNotNull { m ->
                val value = m.optString("value")
                if (value.isEmpty()) null else Option(value, m.optString("label").ifEmpty { value })
            }
            if (parsed.isNotEmpty()) modes = parsed
            if (!livePicture) picture = p.optString("mode_raw")
        }
        if (!liveApp) app = root.optString("input_source").ifEmpty { root.optString("app") }
        if (systemOn == null) {
            root.optJSONObject("powerState")?.let { p -> if (p.has("systemOn")) systemOn = p.optBoolean("systemOn") }
        }
    }

    companion object {
        /** Liście, które subskrybujemy pod prefiksem Glasshouse. */
        val leaves = listOf(
            "status", "telemetry", "state/application/app", "state/picture/pictureMode", "state/power/systemOn",
        )
    }
}
