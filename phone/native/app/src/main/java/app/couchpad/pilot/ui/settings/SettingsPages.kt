package app.couchpad.pilot.ui.settings

import android.os.SystemClock
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Check
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.input.KeyboardType
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import app.couchpad.pilot.BuildConfig
import app.couchpad.pilot.data.BrokerSettings
import app.couchpad.pilot.data.LinkPhase
import app.couchpad.pilot.data.LinkState
import app.couchpad.pilot.data.Options
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.ui.components.Button
import app.couchpad.pilot.ui.components.ConfirmDialog

/** Zmiana preferencji: zapis od razu, jak przełączniki na płytce. */
private typealias Update = ((Preferences) -> Preferences) -> Unit

/** Indeks wartości w tabeli opcji albo [fallback], gdy zapisanej wartości nie ma w tabeli. */
private fun <T> List<T>.indexOr(value: T, fallback: Int) = indexOf(value).takeIf { it >= 0 } ?: fallback

@Composable
internal fun WifiPage() {
    val ip by rememberLocalIp()
    InfoRow("Sieć", if (ip != null) "połączono" else "brak sieci", ip != null)
    InfoRow("Adres IP", ip ?: "—")
    Note("Siecią Wi-Fi zarządza system telefonu (Ustawienia Androida → Połączenia).")
}

@Composable
internal fun DisplayPage(prefs: Preferences, update: Update) {
    Choose("Orientacja", Options.orientationLabels, prefs.orientation) { i -> update { it.copy(orientation = i) } }
    Note("Auto obraca pilota razem z telefonem. Poziom ma zakładki w bocznym pasku.")
    Choose("Styl", Options.themeLabels, prefs.theme) { i -> update { it.copy(theme = i) } }
    if (prefs.theme == 2) {
        Choose("Styl ciemny w godzinach", Options.themeHoursLabels, prefs.themeHours) { i -> update { it.copy(themeHours = i) } }
    }
    Choose("Jasność", Options.brightness.map { "$it%" }, Options.brightness.indexOr(prefs.brightness, 0)) { i ->
        update { it.copy(brightness = Options.brightness[i]) }
    }
    Choose("Tryb nocny", Options.nightLabels, prefs.night) { i -> update { it.copy(night = i) } }
    Note("W nocy jasność najwyżej ${Options.nightBrightness}%.")
    Choose("Wygaszanie po bezczynności", Options.timeoutLabels, prefs.timeout) { i -> update { it.copy(timeout = i) } }
    Choose("Przyciemnianie przed wygaszeniem", listOf("wyłączone", "10 s przed"), if (prefs.dimBeforeSleep) 1 else 0) { i ->
        update { it.copy(dimBeforeSleep = i == 1) }
    }
}

@Composable
internal fun SoundPage(prefs: Preferences, update: Update) {
    Choose(
        "Najwyższa głośność Z906 z tarczy", Options.volumeLimitLabels,
        Options.volumeLimits.indexOr(prefs.volumeLimit, Options.volumeLimits.lastIndex),
    ) { i -> update { it.copy(volumeLimit = Options.volumeLimits[i]) } }
    Note("Tarcza nie wyśle więcej niż limit. Głośności ustawionej inaczej (pilot Logitech, aplikacja) pilot nie zmienia.")
}

@Composable
internal fun PlacePage(prefs: Preferences, update: Update) {
    var room by rememberSaveable(prefs.roomName) { mutableStateOf(prefs.roomName) }
    Field("Nazwa pokoju", room, { room = it })
    Note("W nagłówku ekranów TV, Apple TV, Dźwięk, Dom.")
    Button("Zapisz nazwę", Modifier.fillMaxWidth()) {
        update { it.copy(roomName = room.trim().ifEmpty { "SALON" }.take(24)) }
    }
    Choose("Strefa czasowa", Options.timeZoneLabels, prefs.timeZone) { i -> update { it.copy(timeZone = i) } }
}

@Composable
internal fun KeyboardPage(prefs: Preferences, update: Update) {
    Note("Tekst wpisujesz klawiaturą telefonu.")
    Choose("Tekst na Apple TV", listOf("na żywo", "po zapisie"), if (prefs.keyboardLive) 0 else 1) { i ->
        update { it.copy(keyboardLive = i == 0) }
    }
    Choose("Klawiatura przy polu tekstowym na Apple TV", listOf("sama", "przyciskiem"), if (prefs.keyboardAutoOpen) 0 else 1) { i ->
        update { it.copy(keyboardAutoOpen = i == 0) }
    }
    Choose("Czułość gładzika", Options.trackpadLabels, Options.trackpadSteps.indexOr(prefs.trackpadStep, 1)) { i ->
        update { it.copy(trackpadStep = Options.trackpadSteps[i]) }
    }
}

/** Opis łącza z domem: stan połączenia, a po połączeniu — czy przyszedł świeży stan. */
private fun linkText(link: LinkState, freshDetail: Boolean): String = when (link.phase) {
    LinkPhase.Connected -> if (!freshDetail) "połączono" else if (link.fresh) "gotowe" else "brak świeżego stanu"
    LinkPhase.Connecting -> if (freshDetail) "łączenie" else "łączenie…"
    LinkPhase.Error -> link.error ?: "błąd"
    LinkPhase.Idle -> "brak"
}

@Composable
internal fun DiagnosticsPage(state: PilotState) {
    val ip by rememberLocalIp()
    val home = state.home
    fun known(known: Boolean, available: Boolean) = if (!known) "brak stanu" else if (available) "dostępne" else "niedostępne"
    InfoRow("Adres IP", ip ?: "—")
    InfoRow("Łącze z domem", linkText(state.link, freshDetail = true), state.link.connected)
    InfoRow("Z906", known(state.z906.known, state.z906.available), state.z906.available)
    InfoRow("Telewizor", if (!state.tv.known) "brak stanu" else state.tv.status.ifEmpty { "dostępny" }, state.tv.available)
    InfoRow("Apple TV", known(home.known, home.appleTv.available), home.appleTv.available)
    InfoRow("Home Assistant", if (home.known) "stan świeży" else "brak", home.known)
    InfoRow("Rewizja stanu", if (home.revision >= 0) home.revision.toString() else "—")
    InfoRow("Kanały TV", state.channels.size.toString(), state.channels.isNotEmpty())
}

@Composable
internal fun MqttPage(state: PilotState, repo: PilotRepository) {
    val broker by repo.broker.collectAsStateWithLifecycle()
    var host by rememberSaveable(broker) { mutableStateOf(broker.host) }
    var port by rememberSaveable(broker) { mutableStateOf(broker.port.toString()) }
    var user by rememberSaveable(broker) { mutableStateOf(broker.username) }
    var pass by rememberSaveable(broker) { mutableStateOf(broker.password) }
    var z906 by rememberSaveable(broker) { mutableStateOf(broker.z906Id) }
    var remote by rememberSaveable(broker) { mutableStateOf(broker.remoteId) }
    var tv by rememberSaveable(broker) { mutableStateOf(broker.tvPrefix) }
    var saved by remember { mutableStateOf(false) }
    /** Każda zmiana pola cofa „Zapisano”. */
    fun edit(set: (String) -> Unit): (String) -> Unit = { set(it); saved = false }

    InfoRow("Stan", linkText(state.link, freshDetail = false), state.link.connected)
    Field("Adres brokera", host, edit { host = it }, keyboard = KeyboardType.Uri)
    Field("Port", port, edit { port = it.filter(Char::isDigit) }, keyboard = KeyboardType.Number)
    Field("Użytkownik", user, edit { user = it })
    Field("Hasło", pass, edit { pass = it }, password = true)
    Field("Kontroler Z906 (z906_<mac>, puste = wykryj)", z906, edit { z906 = it })
    Field("Identyfikator pilota (puste = wykryj)", remote, edit { remote = it })
    Field("Telewizor — prefiks Glasshouse (puste = bez TV)", tv, edit { tv = it })
    Button(if (saved) "Zapisano — łączę" else "Zapisz i połącz", Modifier.fillMaxWidth(), icon = Icons.Rounded.Check, accent = true) {
        repo.saveBroker(
            BrokerSettings(
                host = host.trim(), port = port.toIntOrNull() ?: 1883, username = user.trim(), password = pass,
                z906Id = z906.trim(), remoteId = remote.trim(), tvPrefix = tv.trim(),
            )
        )
        saved = true
    }
}

private enum class AboutConfirm { Restart, Reset }

@Composable
internal fun AboutPage(repo: PilotRepository) {
    var confirm by remember { mutableStateOf<AboutConfirm?>(null) }
    val runtime = Runtime.getRuntime()
    InfoRow("Wersja", BuildConfig.VERSION_NAME)
    InfoRow("Wolna pamięć", "${(runtime.maxMemory() - runtime.totalMemory() + runtime.freeMemory()) / 1_048_576} MB")
    InfoRow("Czas pracy", "${SystemClock.elapsedRealtime() / 60_000} min")
    Button("Uruchom ponownie", Modifier.fillMaxWidth()) { confirm = AboutConfirm.Restart }
    Button("Przywróć ustawienia fabryczne", Modifier.fillMaxWidth()) { confirm = AboutConfirm.Reset }
    val close = { confirm = null }
    when (confirm) {
        AboutConfirm.Restart -> ConfirmDialog(
            "Uruchomić ponownie?", "Pilot połączy się z domem od nowa. Ustawienia zostają.", "Uruchom", close, repo::restart,
        )
        AboutConfirm.Reset -> ConfirmDialog(
            "Przywrócić ustawienia fabryczne?",
            "Pilot zapomni broker MQTT i wszystkie ustawienia. Trzeba będzie wpisać je od nowa.",
            "Przywróć", close, repo::factoryReset,
        )
        null -> Unit
    }
}
