package app.couchpad.pilot.data

/**
 * Tabele opcji ekranu ustawień — te same wartości i podpisy co w firmware
 * (`storage::Pref`, components/ui/screens/screen_device.cpp). [Preferences] trzyma indeksy.
 */
object Options {
    val brightness = listOf(20, 40, 60, 80, 100)
    val timeouts = listOf(15_000L, 30_000L, 60_000L, 120_000L, 0L)
    val timeoutLabels = listOf("15 s", "30 s", "1 min", "2 min", "nigdy")

    /** Tryb nocny: godziny od–do (koniec wyłącznie); w nocy jasność najwyżej [nightBrightness]. */
    val nightWindows = listOf(0 to 0, 21 to 7, 22 to 7, 23 to 6)
    val nightLabels = listOf("wył.", "21–7", "22–7", "23–6")
    const val nightBrightness = 20

    val themeLabels = listOf("ciemny", "jasny", "wg godzin")
    val themeHours = listOf(17 to 7, 18 to 8, 19 to 7, 21 to 7)
    val themeHoursLabels = listOf("17–7", "18–8", "19–7", "21–7")
    val orientationLabels = listOf("auto", "pion", "poziom", "poziom odwr.")
    val timeZones = listOf("Europe/Warsaw", "UTC", "Europe/London", "Europe/Kiev")
    val timeZoneLabels = listOf("Polska", "UTC", "Londyn", "Kijów")
    val trackpadSteps = listOf(20, 26, 34)
    val trackpadLabels = listOf("czuły", "zwykły", "spokojny")
    val volumeLimits = listOf(50, 60, 70, 80, 100)
    val volumeLimitLabels = listOf("50%", "60%", "70%", "80%", "bez")

    /** Czy godzina wypada w oknie od–do; okno może przechodzić przez północ. */
    fun hourIn(window: Pair<Int, Int>, hour: Int): Boolean {
        val (from, to) = window
        if (from == to) return false
        return if (from < to) hour in from until to else hour >= from || hour < to
    }
}

/** Strefa czasowa wybrana w Pokój i czas. */
val Preferences.zoneId: String get() = Options.timeZones.getOrElse(timeZone) { Options.timeZones[0] }
