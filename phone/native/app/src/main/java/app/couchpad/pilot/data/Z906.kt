package app.couchpad.pilot.data

/**
 * Logitech Z906 przez kontroler na tematach `z906/<id>/…` (ADR 0006). Wejścia i efekty są
 * stałe dla modelu i co do znaku takie jak w tematach kontrolera.
 */
object Z906 {
    val sources = listOf("PC - analog 5.1", "RCA - stereo", "Optical 1", "Optical 2", "Coaxial", "AUX")
    val effects = listOf("Stereo 3D", "Stereo 2.1", "Stereo 4.1", "No effect")

    fun effectLabel(effect: String) = if (effect == "No effect") "Bez efektu" else effect

    /** 44 poziomy wzmacniacza (0–43): −/+ liczą w poziomach, nie w procentach. */
    const val maxLevel = 43

    private fun levelOf(percent: Int) = (percent * maxLevel + 50) / 100
    private fun percentOf(level: Int) = (level * 100 + maxLevel / 2) / maxLevel

    /**
     * Głośność po kroku o [levels] poziomów wzmacniacza, nie wyżej niż [limit] procent.
     * Krok nigdy nie cofa wartości w przeciwną stronę (np. przy wartości nad limitem).
     */
    fun stepVolume(percent: Int, levels: Int, limit: Int): Int {
        val level = (levelOf(percent) + levels).coerceIn(0, maxLevel)
        var next = percentOf(level)
        if (next > limit) next = percentOf(limit * maxLevel / 100)
        if (levels > 0 && next < percent) return percent
        if (levels < 0 && next > percent) return percent
        return next
    }
}

/** Kanały poziomów: klucz w UI i liść tematu kontrolera (ten pisze „centre”). */
enum class Z906Channel(val key: String, val topic: String, val label: String) {
    Center("center", "centre", "Center"),
    Subwoofer("subwoofer", "subwoofer", "Subwoofer"),
    Rear("rear", "rear", "Tył"),
}

/** Stan po wiadomości z liścia [leaf] tematu kontrolera albo null, gdy liść nie jest znany. */
internal fun Z906State.withTopic(leaf: String, value: String): Z906State? {
    val on = value.equals("ON", ignoreCase = true)
    val next = when (leaf) {
        "availability" -> return copy(available = value == "online")
        "state/power" -> copy(power = on)
        "state/mute" -> copy(muted = on)
        "state/volume" -> copy(volume = value.toIntOrNull())
        "state/centre" -> copy(center = value.toIntOrNull())
        "state/subwoofer" -> copy(subwoofer = value.toIntOrNull())
        "state/rear" -> copy(rear = value.toIntOrNull())
        "state/source" -> copy(source = value)
        "state/effect" -> copy(effect = value)
        "state/decode" -> copy(decode = on)
        else -> return null
    }
    // Każdy stan od kontrolera oznacza, że jest dostępny.
    return next.copy(available = true)
}

fun Z906State.level(channel: Z906Channel): Int? = when (channel) {
    Z906Channel.Center -> center
    Z906Channel.Subwoofer -> subwoofer
    Z906Channel.Rear -> rear
}
