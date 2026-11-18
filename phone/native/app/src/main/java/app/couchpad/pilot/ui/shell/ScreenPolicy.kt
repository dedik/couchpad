package app.couchpad.pilot.ui.shell

import app.couchpad.pilot.data.Options
import app.couchpad.pilot.data.Preferences

/**
 * Reguły ekranu z ustawień, jak `ui.cpp` na płytce: styl wg godzin (ADR 0012), tryb nocny,
 * przyciemnienie przed wygaszeniem i wygaszanie po bezczynności. Czyste funkcje.
 */
internal object ScreenPolicy {
    /** Przyciemnienie przed wygaszeniem (ui.cpp: kDimPercent) i jak długo przed nim. */
    const val DIM_PERCENT = 15
    const val DIM_BEFORE_MS = 10_000L

    /** Jasność uśpionego pilota pod zasłoną ([Preferences.sleepOverlay]): wygaszony do 0%. */
    const val ASLEEP_PERCENT = 0

    fun dark(prefs: Preferences, hour: Int): Boolean = when (prefs.theme) {
        1 -> false
        2 -> Options.hourIn(Options.themeHours.getOrElse(prefs.themeHours) { 18 to 8 }, hour)
        else -> true
    }

    /** Czas bezczynności do wygaszenia; 0 = nigdy. */
    fun timeout(prefs: Preferences): Long = Options.timeouts.getOrElse(prefs.timeout) { 30_000L }

    fun shouldSleep(prefs: Preferences, idleMs: Long): Boolean {
        val timeout = timeout(prefs)
        return timeout > 0 && idleMs > timeout
    }

    /**
     * Uśpienie systemowe: czy pilot trzyma jeszcze ekran włączony. Zwolnienie blokady ekranu
     * liczy się w Androidzie jak dotyk, więc pilot puszcza ekran o czas wygaszania systemu
     * wcześniej — gaśnie wtedy po dłuższym z dwóch czasów, a nie po ich sumie.
     */
    fun holdScreen(prefs: Preferences, idleMs: Long, systemTimeoutMs: Long): Boolean {
        val timeout = timeout(prefs)
        return timeout == 0L || idleMs < timeout - systemTimeoutMs
    }

    fun dimmed(prefs: Preferences, idleMs: Long): Boolean {
        val timeout = timeout(prefs)
        return timeout > 0 && prefs.dimBeforeSleep && idleMs > timeout - DIM_BEFORE_MS
    }

    fun brightness(prefs: Preferences, hour: Int, asleep: Boolean, dimmed: Boolean): Int {
        val night = Options.nightWindows.getOrElse(prefs.night) { 0 to 0 }
        val chosen = if (Options.hourIn(night, hour)) minOf(prefs.brightness, Options.nightBrightness) else prefs.brightness
        return when {
            asleep -> ASLEEP_PERCENT
            dimmed -> minOf(chosen, DIM_PERCENT)
            else -> chosen
        }
    }
}
