package app.couchpad.pilot.data

/**
 * Dane brokera. Są sekretem użytkownika: powstają na telefonie i nie trafiają do
 * repozytorium ani logów.
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

    /** Prefiks tematów kontrolera Z906 i kontraktu v1 tego pilota. */
    val z906Base get() = "z906/$z906Id"
    val v1Base get() = "${PilotRepository.V1}/$remoteId"
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
    /**
     * Ciemny styl w wariancie OLED (tylko telefon, strona Telefon; płytka ma ekran IPS).
     */
    val oled: Boolean = false,
)
