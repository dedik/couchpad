package app.couchpad.pilot.data

/**
 * Najczęściej wybierane efekty lampy (tylko aplikacja; panel pokazuje listę). Liczniki żyją
 * w telefonie ([SettingsStore]), nie w kontrakcie MQTT.
 */
object EffectUsage {
    /**
     * Do [n] efektów z [effects] poza [exclude], od najczęściej wybieranych. Remis (także
     * brak licznika) rozstrzyga kolejność z listy Home Assistant.
     */
    fun frequent(effects: List<String>, usage: Map<String, Int>, exclude: String, n: Int): List<String> =
        effects.withIndex()
            .filter { it.value.isNotEmpty() && it.value != exclude }
            .sortedWith(compareByDescending<IndexedValue<String>> { usage[it.value] ?: 0 }.thenBy { it.index })
            .take(n.coerceAtLeast(0))
            .map { it.value }
}
