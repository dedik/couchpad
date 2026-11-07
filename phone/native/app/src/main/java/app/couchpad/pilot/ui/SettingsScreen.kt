package app.couchpad.pilot.ui

import android.os.SystemClock
import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.imePadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.Check
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Keyboard
import androidx.compose.material.icons.rounded.List
import androidx.compose.material.icons.rounded.Navigation
import androidx.compose.material.icons.rounded.Repeat
import androidx.compose.material.icons.rounded.Visibility
import androidx.compose.material.icons.rounded.VolumeUp
import androidx.compose.material.icons.rounded.Wifi
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.window.Dialog
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import app.couchpad.pilot.BuildConfig
import app.couchpad.pilot.R
import app.couchpad.pilot.data.BrokerSettings
import app.couchpad.pilot.data.LinkPhase
import app.couchpad.pilot.data.Options
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import java.net.Inet4Address
import java.net.NetworkInterface

private enum class Page(val title: String, val note: String, val icon: ImageVector) {
    Wifi("Wi-Fi", "sieć telefonu", Icons.Rounded.Wifi),
    Mqtt("MQTT", "multimedia_remote/v1", Icons.Rounded.Repeat),
    Display("Ekran", "styl, jasność, wygaszanie", Icons.Rounded.Visibility),
    Sound("Dźwięk", "limit głośności Z906", Icons.Rounded.VolumeUp),
    Place("Pokój i czas", "nazwa pokoju, strefa czasowa", Icons.Rounded.Navigation),
    Keyboard("Klawiatura i gładzik", "tekst na żywo, czułość", Icons.Rounded.Keyboard),
    Diagnostics("Diagnostyka", "IP, łącze z domem, stan urządzeń", Icons.Rounded.List),
    About("O urządzeniu", "wersja, restart, ustawienia fabryczne", Icons.Rounded.Info),
}

@Composable
fun SettingsScreen(state: PilotState, repo: PilotRepository) {
    val prefs by repo.prefs.collectAsStateWithLifecycle()
    val landscape = LocalLandscape.current
    var pageName by rememberSaveable { mutableStateOf<String?>(if (landscape) Page.Display.name else null) }
    val page = pageName?.let { Page.valueOf(it) }

    if (landscape) {
        Row(Modifier.fillMaxSize()) {
            Column(Modifier.weight(0.45f).fillMaxHeight()) {
                Header(R.drawable.px_settings, "Pilot", "Ustawienia")
                PageList(state, prefs, page, Modifier.weight(1f)) { pageName = it.name }
            }
            Box(Modifier.weight(0.55f).fillMaxHeight()) {
                if (page != null) PageContent(page, state, repo, prefs, back = null)
            }
        }
    } else {
        if (page != null) {
            BackHandler { pageName = null }
            PageContent(page, state, repo, prefs) { pageName = null }
        } else {
            Column(Modifier.fillMaxSize()) {
                Header(R.drawable.px_settings, "Pilot", "Ustawienia")
                PageList(state, prefs, null, Modifier.weight(1f)) { pageName = it.name }
            }
        }
    }
}

@Composable
private fun PageList(state: PilotState, prefs: Preferences, selected: Page?, modifier: Modifier, open: (Page) -> Unit) {
    Column(
        modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 24.u, vertical = 4.u),
        verticalArrangement = Arrangement.spacedBy(10.u),
    ) {
        for (p in Page.values()) {
            val (value, ok) = when (p) {
                Page.Wifi -> (if (localIp() != null) "połączono" else "brak sieci") to (localIp() != null)
                Page.Mqtt -> (if (state.link.phase == LinkPhase.Connected) "połączono" else "brak") to (state.link.phase == LinkPhase.Connected)
                Page.Display -> "${Options.themeLabels[prefs.theme]} · ${prefs.brightness}% · ${Options.timeoutLabels[prefs.timeout]}" to false
                Page.Sound -> (if (prefs.volumeLimit >= 100) "bez limitu" else "maks. ${prefs.volumeLimit}%") to false
                Page.Place -> prefs.roomName to false
                else -> "" to false
            }
            Card(Modifier.fillMaxWidth().height(64.u), radius = 16, selected = p == selected, onClick = { open(p) }) {
                Row(Modifier.fillMaxSize().padding(horizontal = 16.u), verticalAlignment = Alignment.CenterVertically) {
                    Icon(p.icon, null, tint = C.dim, modifier = Modifier.size(22.u))
                    Spacer(Modifier.width(14.u))
                    Column(Modifier.weight(1f)) {
                        T(p.title, 15, FontWeight.Bold)
                        T(p.note, 11, color = C.faint)
                    }
                    if (value.isNotEmpty()) T(value, 12, color = if (ok) C.ok else C.dim)
                    Icon(Icons.Rounded.ChevronRight, null, tint = C.faint, modifier = Modifier.size(22.u))
                }
            }
        }
        Spacer(Modifier.height(12.u))
    }
}

@Composable
private fun PageContent(page: Page, state: PilotState, repo: PilotRepository, prefs: Preferences, back: (() -> Unit)?) {
    Column(Modifier.fillMaxSize()) {
        Row(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 12.u), verticalAlignment = Alignment.CenterVertically) {
            if (back != null) {
                RoundButton(Icons.Rounded.ArrowBackIosNew, size = 44, onClick = back)
                Spacer(Modifier.width(14.u))
            }
            T(page.title, 22, FontWeight.Bold)
        }
        Column(
            Modifier.fillMaxSize().imePadding().verticalScroll(rememberScrollState()).padding(horizontal = 24.u),
            verticalArrangement = Arrangement.spacedBy(16.u),
        ) {
            val update = repo::updatePreferences
            when (page) {
                Page.Wifi -> {
                    InfoRow("Sieć", localIp()?.let { "połączono" } ?: "brak sieci", localIp() != null)
                    InfoRow("Adres IP", localIp() ?: "—", false)
                    Note("Siecią Wi-Fi zarządza system telefonu (Ustawienia Androida → Połączenia).")
                }
                Page.Mqtt -> MqttPage(state, repo)
                Page.Display -> {
                    Group("Orientacja") {
                        Segmented(Options.orientationLabels, prefs.orientation) { i -> update { it.copy(orientation = i) } }
                    }
                    Note("Auto obraca pilota razem z telefonem. Poziom ma zakładki w bocznym pasku.")
                    Group("Styl") {
                        Segmented(Options.themeLabels, prefs.theme) { i -> update { it.copy(theme = i) } }
                    }
                    if (prefs.theme == 2) {
                        Group("Styl ciemny w godzinach") {
                            Segmented(Options.themeHoursLabels, prefs.themeHours) { i -> update { it.copy(themeHours = i) } }
                        }
                    }
                    Group("Jasność") {
                        Segmented(Options.brightness.map { "$it%" }, Options.brightness.indexOf(prefs.brightness).coerceAtLeast(0)) { i ->
                            update { it.copy(brightness = Options.brightness[i]) }
                        }
                    }
                    Group("Tryb nocny") {
                        Segmented(Options.nightLabels, prefs.night) { i -> update { it.copy(night = i) } }
                    }
                    Note("W nocy jasność najwyżej ${Options.nightBrightness}%.")
                    Group("Wygaszanie po bezczynności") {
                        Segmented(Options.timeoutLabels, prefs.timeout) { i -> update { it.copy(timeout = i) } }
                    }
                    Group("Przyciemnianie przed wygaszeniem") {
                        Segmented(listOf("wyłączone", "10 s przed"), if (prefs.dimBeforeSleep) 1 else 0) { i ->
                            update { it.copy(dimBeforeSleep = i == 1) }
                        }
                    }
                }
                Page.Sound -> {
                    Group("Najwyższa głośność Z906 z tarczy") {
                        Segmented(Options.volumeLimitLabels, Options.volumeLimits.indexOf(prefs.volumeLimit).coerceAtLeast(4)) { i ->
                            update { it.copy(volumeLimit = Options.volumeLimits[i]) }
                        }
                    }
                    Note("Tarcza nie wyśle więcej niż limit. Głośności ustawionej inaczej (pilot Logitech, aplikacja) pilot nie zmienia.")
                }
                Page.Place -> PlacePage(repo, prefs)
                Page.Keyboard -> {
                    Note("Tekst wpisujesz klawiaturą telefonu.")
                    Group("Tekst na Apple TV") {
                        Segmented(listOf("na żywo", "po zapisie"), if (prefs.keyboardLive) 0 else 1) { i ->
                            update { it.copy(keyboardLive = i == 0) }
                        }
                    }
                    Group("Klawiatura przy polu tekstowym na Apple TV") {
                        Segmented(listOf("sama", "przyciskiem"), if (prefs.keyboardAutoOpen) 0 else 1) { i ->
                            update { it.copy(keyboardAutoOpen = i == 0) }
                        }
                    }
                    Group("Czułość gładzika") {
                        Segmented(Options.trackpadLabels, Options.trackpadSteps.indexOf(prefs.trackpadStep).coerceAtLeast(1)) { i ->
                            update { it.copy(trackpadStep = Options.trackpadSteps[i]) }
                        }
                    }
                }
                Page.Diagnostics -> {
                    InfoRow("Adres IP", localIp() ?: "—", false)
                    InfoRow(
                        "Łącze z domem",
                        when (state.link.phase) {
                            LinkPhase.Connected -> if (state.link.freshSince != 0L) "gotowe" else "brak świeżego stanu"
                            LinkPhase.Connecting -> "łączenie"
                            LinkPhase.Error -> state.link.error ?: "błąd"
                            LinkPhase.Idle -> "brak"
                        },
                        state.link.phase == LinkPhase.Connected,
                    )
                    InfoRow("Z906", if (!state.z906.known) "brak stanu" else if (state.z906.available) "dostępne" else "niedostępne", state.z906.available)
                    InfoRow("Telewizor", if (!state.tv.known) "brak stanu" else state.tv.status.ifEmpty { "dostępny" }, state.tv.available)
                    InfoRow("Apple TV", if (!state.home.known) "brak stanu" else if (state.home.appleTv.available) "dostępne" else "niedostępne", state.home.appleTv.available)
                    InfoRow("Home Assistant", if (state.home.known) "stan świeży" else "brak", state.home.known)
                    InfoRow("Rewizja stanu", if (state.home.revision >= 0) state.home.revision.toString() else "—", false)
                    InfoRow("Kanały TV", state.channels.size.toString(), state.channels.isNotEmpty())
                }
                Page.About -> AboutPage(repo)
            }
            Spacer(Modifier.height(24.u))
        }
    }
}

@Composable
private fun MqttPage(state: PilotState, repo: PilotRepository) {
    val broker by repo.broker.collectAsStateWithLifecycle()
    var host by rememberSaveable(broker) { mutableStateOf(broker.host) }
    var port by rememberSaveable(broker) { mutableStateOf(broker.port.toString()) }
    var user by rememberSaveable(broker) { mutableStateOf(broker.username) }
    var pass by rememberSaveable(broker) { mutableStateOf(broker.password) }
    var z906 by rememberSaveable(broker) { mutableStateOf(broker.z906Id) }
    var remote by rememberSaveable(broker) { mutableStateOf(broker.remoteId) }
    var tv by rememberSaveable(broker) { mutableStateOf(broker.tvPrefix) }
    var saved by remember { mutableStateOf(false) }
    InfoRow(
        "Stan",
        when (state.link.phase) {
            LinkPhase.Connected -> "połączono"
            LinkPhase.Connecting -> "łączenie…"
            LinkPhase.Error -> state.link.error ?: "błąd"
            LinkPhase.Idle -> "brak"
        },
        state.link.phase == LinkPhase.Connected,
    )
    Field("Adres brokera", host, { host = it; saved = false }, keyboard = KeyboardType.Uri)
    Field("Port", port, { port = it.filter(Char::isDigit); saved = false }, keyboard = KeyboardType.Number)
    Field("Użytkownik", user, { user = it; saved = false })
    Field("Hasło", pass, { pass = it; saved = false }, password = true)
    Field("Kontroler Z906 (z906_<mac>, puste = wykryj)", z906, { z906 = it; saved = false })
    Field("Identyfikator pilota (puste = wykryj)", remote, { remote = it; saved = false })
    Field("Telewizor — prefiks Glasshouse (puste = bez TV)", tv, { tv = it; saved = false })
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

@Composable
private fun PlacePage(repo: PilotRepository, prefs: Preferences) {
    var room by rememberSaveable(prefs.roomName) { mutableStateOf(prefs.roomName) }
    Field("Nazwa pokoju", room, { room = it })
    Note("W nagłówku ekranów TV, Apple TV, Dźwięk, Dom.")
    Button("Zapisz nazwę", Modifier.fillMaxWidth()) {
        repo.updatePreferences { it.copy(roomName = room.trim().ifEmpty { "SALON" }.take(24)) }
    }
    Group("Strefa czasowa") {
        Segmented(Options.timeZoneLabels, prefs.timeZone) { i -> repo.updatePreferences { it.copy(timeZone = i) } }
    }
}

@Composable
private fun AboutPage(repo: PilotRepository) {
    var confirm by remember { mutableStateOf<String?>(null) }
    val runtime = Runtime.getRuntime()
    InfoRow("Wersja", BuildConfig.VERSION_NAME, false)
    InfoRow("Wolna pamięć", "${(runtime.maxMemory() - runtime.totalMemory() + runtime.freeMemory()) / 1_048_576} MB", false)
    InfoRow("Czas pracy", "${SystemClock.elapsedRealtime() / 60_000} min", false)
    Button("Uruchom ponownie", Modifier.fillMaxWidth()) { confirm = "restart" }
    Button("Przywróć ustawienia fabryczne", Modifier.fillMaxWidth()) { confirm = "reset" }
    when (confirm) {
        "restart" -> Confirm(
            "Uruchomić ponownie?", "Pilot połączy się z domem od nowa. Ustawienia zostają.", "Uruchom",
            { confirm = null },
        ) { repo.restart() }
        "reset" -> Confirm(
            "Przywrócić ustawienia fabryczne?",
            "Pilot zapomni broker MQTT i wszystkie ustawienia. Trzeba będzie wpisać je od nowa.",
            "Przywróć",
            { confirm = null },
        ) { repo.factoryReset() }
    }
}

@Composable
private fun Confirm(title: String, text: String, action: String, dismiss: () -> Unit, onConfirm: () -> Unit) {
    Dialog(onDismissRequest = dismiss) {
        ProvideUnits {
            Column(
                Modifier.fillMaxWidth().clip(RoundedCornerShape(24.u)).background(C.surface)
                    .border(1.u, C.line, RoundedCornerShape(24.u)).padding(22.u),
                verticalArrangement = Arrangement.spacedBy(14.u),
            ) {
                T(title, 19, FontWeight.Bold)
                Text(text, style = style(14, color = C.dim))
                Row(horizontalArrangement = Arrangement.spacedBy(12.u)) {
                    Button("Anuluj", Modifier.weight(1f), onClick = dismiss)
                    Button(action, Modifier.weight(1f), accent = true) { onConfirm(); dismiss() }
                }
            }
        }
    }
}

/** Przełącznik segmentowy: jedna opcja z kilku, jak na płytce. */
@Composable
fun Segmented(labels: List<String>, selected: Int, onSelect: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().height(48.u).clip(RoundedCornerShape(14.u)).background(C.surface)
            .border(1.u, C.line, RoundedCornerShape(14.u)).padding(4.u),
    ) {
        labels.forEachIndexed { i, label ->
            val active = i == selected
            Box(
                Modifier.weight(1f).fillMaxHeight().clip(RoundedCornerShape(10.u))
                    .background(if (active) C.surface2 else Color.Transparent)
                    .border(1.u, if (active) C.accent else Color.Transparent, RoundedCornerShape(10.u))
                    .clickable(remember { MutableInteractionSource() }, null) { haptic(); onSelect(i) },
                contentAlignment = Alignment.Center,
            ) {
                T(label, 13, if (active) FontWeight.Bold else FontWeight.Normal, if (active) C.text else C.dim)
            }
        }
    }
}

@Composable
private fun Group(title: String, content: @Composable () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(8.u)) {
        T(title.uppercase(), 11, FontWeight.Medium, C.faint)
        content()
    }
}

@Composable
private fun Note(text: String) {
    Text(text, style = style(12, color = C.faint), textAlign = TextAlign.Start)
}

@Composable
private fun InfoRow(label: String, value: String, ok: Boolean) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        T(label, 15, FontWeight.Medium, modifier = Modifier.weight(1f))
        T(value, 13, color = if (ok) C.ok else C.dim)
    }
}

@Composable
private fun Field(
    label: String,
    value: String,
    onChange: (String) -> Unit,
    keyboard: KeyboardType = KeyboardType.Text,
    password: Boolean = false,
) {
    Column {
        T(label, 12, color = C.dim)
        Spacer(Modifier.height(6.u))
        BasicTextField(
            value = value,
            onValueChange = onChange,
            singleLine = true,
            textStyle = style(16),
            cursorBrush = SolidColor(C.accent),
            visualTransformation = if (password) PasswordVisualTransformation() else VisualTransformation.None,
            keyboardOptions = KeyboardOptions(
                keyboardType = if (password) KeyboardType.Password else keyboard,
                autoCorrect = false,
            ),
            modifier = Modifier.fillMaxWidth().clip(RoundedCornerShape(12.u)).background(C.surface2)
                .border(1.u, C.line, RoundedCornerShape(12.u)).padding(horizontal = 14.u, vertical = 12.u),
        )
    }
}

private fun localIp(): String? = runCatching {
    NetworkInterface.getNetworkInterfaces().toList()
        .filter { it.isUp && !it.isLoopback }
        .flatMap { it.inetAddresses.toList() }
        .firstOrNull { it is Inet4Address && !it.isLoopbackAddress }?.hostAddress
}.getOrNull()
