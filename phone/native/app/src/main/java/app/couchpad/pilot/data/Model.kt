package app.couchpad.pilot.data

/** Stan łącza do domu, jak trzy kropki na ekranie łączenia firmware'u. */
enum class LinkPhase { Idle, Connecting, Connected, Error }

data class LinkState(
    val phase: LinkPhase = LinkPhase.Idle,
    val error: String? = null,
    /** Czas (ms) ostatniego potwierdzonego stanu po wybudzeniu — 0, gdy brak świeżego. */
    val freshSince: Long = 0,
)

data class Z906State(
    val available: Boolean = false,
    val power: Boolean? = null,
    val muted: Boolean = false,
    val volume: Int? = null,
    val center: Int? = null,
    val subwoofer: Int? = null,
    val rear: Int? = null,
    val source: String = "",
    val effect: String = "",
    val decode: Boolean = false,
    val updatedAt: Long = 0,
) {
    val known get() = updatedAt > 0
}

data class Option(val id: String, val label: String)

data class TvState(
    val available: Boolean = false,
    val status: String = "",
    val power: Boolean = false,
    val sources: List<Option> = emptyList(),
    val source: String = "",            // identyfikator aktywnego wejścia/aplikacji
    val pictureModes: List<Option> = emptyList(),
    val pictureMode: String = "",       // identyfikator trybu
    val updatedAt: Long = 0,
) {
    val known get() = updatedAt > 0
    val sourceLabel get() = sources.firstOrNull { it.id == source }?.label ?: ""
    val pictureLabel get() = pictureModes.firstOrNull { it.id == pictureMode }?.label ?: ""
}

data class Channel(val number: String, val name: String, val id: String)

data class AppleTvApp(val id: String, val label: String)

data class AppleTvState(
    val available: Boolean = false,
    val power: Boolean = false,
    val appId: String = "",
    val apps: List<AppleTvApp> = emptyList(),
    val keyboardActive: Boolean = false,
)

data class ClimateState(
    val available: Boolean = false,
    val hvacMode: String = "off",
    val hvacModes: List<String> = emptyList(),
    val target: Double? = null,
    val current: Double? = null,
    val min: Double = 16.0,
    val max: Double = 31.0,
    val step: Double = 0.5,
    val fanMode: String = "",
    val fanModes: List<String> = emptyList(),
    val swingMode: String = "",
    val swingModes: List<String> = emptyList(),
    val swingHorizontalMode: String = "",
    val swingHorizontalModes: List<String> = emptyList(),
)

data class LightMember(
    val id: String,
    val label: String,
    val available: Boolean,
    val on: Boolean,
    val brightness: Int,
    val kelvin: Int,
)

data class Light(
    val room: String,
    val id: String,
    val label: String,
    val available: Boolean,
    val on: Boolean,
    val dimmable: Boolean,
    val brightness: Int,
    val kelvin: Int,
    val minKelvin: Int,
    val maxKelvin: Int,
    val effect: String,
    val effects: List<String>,
    val members: List<LightMember>,
)

data class HomeSnapshot(
    val revision: Long = -1,
    val appleTv: AppleTvState = AppleTvState(),
    val climate: ClimateState = ClimateState(),
    val lights: List<Light> = emptyList(),
    val receivedAt: Long = 0,
) {
    val known get() = receivedAt > 0
}

data class Weather(
    val condition: String = "",
    val temperature: Double? = null,
    val night: Boolean = false,
    val indoorTemperature: Double? = null,
    val indoorHumidity: Int? = null,
    val receivedAt: Long = 0,
)

/** Wszystko, co pokazuje interfejs. */
data class PilotState(
    val link: LinkState = LinkState(),
    val z906: Z906State = Z906State(),
    val tv: TvState = TvState(),
    val home: HomeSnapshot = HomeSnapshot(),
    val weather: Weather = Weather(),
    val channels: List<Channel> = emptyList(),
    /** Polecenia czekające na potwierdzenie stanem (klucz kontrolki → czas wysłania). */
    val pending: Map<String, Long> = emptyMap(),
)
