package app.couchpad.pilot.ui.climate

/** Nazwy z firmware'u (screen_climate.cpp): identyfikatory HA → polskie podpisy. */
internal object ClimateLabels {
    private val modes = mapOf(
        "off" to "Wyłączona", "heat_cool" to "Auto", "auto" to "Auto", "cool" to "Chłodzenie",
        "heat" to "Grzanie", "dry" to "Osuszanie", "fan_only" to "Wentylator",
    )
    private val captions = mapOf(
        "off" to "WYŁĄCZONA", "heat_cool" to "AUTO DO", "auto" to "AUTO DO", "cool" to "CHŁODZENIE DO",
        "heat" to "GRZANIE DO", "dry" to "OSUSZANIE DO", "fan_only" to "SAM NAWIEW",
    )
    private val fans = mapOf(
        "auto" to "Auto", "quiet" to "Cicha", "low" to "Niska", "medlow" to "Średnio niska",
        "medium" to "Średnia", "middle" to "Średnia", "medhigh" to "Średnio wysoka", "high" to "Wysoka",
        "strong" to "Maksymalna", "top" to "Maksymalna", "on" to "Włączona", "off" to "Wyłączona",
    )
    private val swings = mapOf(
        "off" to "Stałe", "on" to "Ruch", "Full" to "Ruch", "both" to "Oba kierunki", "vertical" to "Pion",
        "horizontal" to "Poziom", "Upper" to "Góra", "Lower" to "Dół", "Left" to "Lewo", "Center" to "Środek",
        "Right" to "Prawo",
    )

    fun mode(id: String) = modes[id] ?: id
    fun caption(id: String) = captions[id] ?: id.uppercase()
    fun fan(id: String) = fans[id] ?: id
    fun swing(id: String) = swings[id] ?: id
}
