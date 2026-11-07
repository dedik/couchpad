package app.couchpad.pilot.data

import android.content.Context

/**
 * Ustawienia pilota w prywatnych SharedPreferences aplikacji. Dane brokera są sekretem
 * użytkownika: powstają na telefonie i nie trafiają do repozytorium ani logów.
 */
data class BrokerSettings(
    val host: String = "",
    val port: Int = 1883,
    val username: String = "",
    val password: String = "",
    /** Identyfikator kontrolera Z906 w tematach, `z906_<mac>` (ADR 0006). */
    val z906Id: String = "",
    /** Identyfikator pilota w `multimedia_remote/v1/<remote_id>` (ADR 0007). */
    val remoteId: String = "",
    /** Prefiks tematów Glasshouse (ADR 0008). Pusty wyłącza telewizor. */
    val tvPrefix: String = "lgtv",
) {
    val complete get() = host.isNotBlank() && port in 1..65535
}

/**
 * Preferencje 1:1 z ekranu ustawień płytki (components/ui/screens/screen_device.cpp).
 * Wartości są indeksami do tabel z [Options], jak `storage::Pref` w firmware.
 */
data class Preferences(
    val roomName: String = "SALON",
    /** 0 auto (telefon), 1 pion, 2 poziom, 3 poziom odwrócony. */
    val orientation: Int = 0,
    /** 0 ciemny, 1 jasny, 2 wg godzin. */
    val theme: Int = 0,
    val themeHours: Int = 1,
    val brightness: Int = 100,
    /** Indeks w [Options.timeouts]; domyślnie 30 s. */
    val timeout: Int = 1,
    val dimBeforeSleep: Boolean = false,
    /** Indeks w [Options.nightWindows]; 0 = wyłączony. */
    val night: Int = 0,
    val timeZone: Int = 0,
    val keyboardLive: Boolean = true,
    val keyboardAutoOpen: Boolean = true,
    val trackpadStep: Int = 26,
    val volumeLimit: Int = 100,
)

/** Tabele opcji — te same wartości i podpisy co w firmware. */
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

    fun hourIn(window: Pair<Int, Int>, hour: Int): Boolean {
        val (from, to) = window
        if (from == to) return false
        return if (from < to) hour in from until to else hour >= from || hour < to
    }
}

class SettingsStore(context: Context) {
    private val prefs = context.getSharedPreferences("pilot", Context.MODE_PRIVATE)

    fun loadBroker() = BrokerSettings(
        host = prefs.getString("host", "") ?: "",
        port = prefs.getInt("port", 1883),
        username = prefs.getString("username", "") ?: "",
        password = prefs.getString("password", "") ?: "",
        z906Id = prefs.getString("z906_id", "") ?: "",
        remoteId = prefs.getString("remote_id", "") ?: "",
        tvPrefix = prefs.getString("tv_prefix", "lgtv") ?: "lgtv",
    )

    fun saveBroker(s: BrokerSettings) {
        prefs.edit()
            .putString("host", s.host.trim())
            .putInt("port", s.port)
            .putString("username", s.username)
            .putString("password", s.password)
            .putString("z906_id", s.z906Id.trim())
            .putString("remote_id", s.remoteId.trim())
            .putString("tv_prefix", s.tvPrefix.trim())
            .apply()
    }

    fun loadPreferences(): Preferences {
        val d = Preferences()
        return Preferences(
            roomName = prefs.getString("room", d.roomName) ?: d.roomName,
            orientation = prefs.getInt("orientation", d.orientation),
            theme = prefs.getInt("theme", d.theme),
            themeHours = prefs.getInt("theme_hours", d.themeHours),
            brightness = prefs.getInt("brightness", d.brightness),
            timeout = prefs.getInt("timeout", d.timeout),
            dimBeforeSleep = prefs.getBoolean("dim_before_sleep", d.dimBeforeSleep),
            night = prefs.getInt("night", d.night),
            timeZone = prefs.getInt("time_zone", d.timeZone),
            keyboardLive = prefs.getBoolean("keyboard_live", d.keyboardLive),
            keyboardAutoOpen = prefs.getBoolean("keyboard_auto", d.keyboardAutoOpen),
            trackpadStep = prefs.getInt("trackpad_step", d.trackpadStep),
            volumeLimit = prefs.getInt("volume_limit", d.volumeLimit),
        )
    }

    fun savePreferences(p: Preferences) {
        prefs.edit()
            .putString("room", p.roomName)
            .putInt("orientation", p.orientation)
            .putInt("theme", p.theme)
            .putInt("theme_hours", p.themeHours)
            .putInt("brightness", p.brightness)
            .putInt("timeout", p.timeout)
            .putBoolean("dim_before_sleep", p.dimBeforeSleep)
            .putInt("night", p.night)
            .putInt("time_zone", p.timeZone)
            .putBoolean("keyboard_live", p.keyboardLive)
            .putBoolean("keyboard_auto", p.keyboardAutoOpen)
            .putInt("trackpad_step", p.trackpadStep)
            .putInt("volume_limit", p.volumeLimit)
            .apply()
    }

    /** Ustawienia fabryczne: broker, identyfikatory i wszystkie preferencje. */
    fun clearAll() {
        prefs.edit().clear().apply()
    }
}
