package app.couchpad.pilot.ui.settings

import androidx.activity.compose.BackHandler
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
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.Info
import androidx.compose.material.icons.rounded.Keyboard
import androidx.compose.material.icons.rounded.List
import androidx.compose.material.icons.rounded.Navigation
import androidx.compose.material.icons.rounded.PhoneAndroid
import androidx.compose.material.icons.rounded.Repeat
import androidx.compose.material.icons.rounded.Visibility
import androidx.compose.material.icons.rounded.VolumeUp
import androidx.compose.material.icons.rounded.Wifi
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Options
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.DeviceHeader
import app.couchpad.pilot.ui.components.PageHeader
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/** Strony ustawień w kolejności z płytki, plus Telefon (launcher). */
internal enum class Page(val title: String, val note: String, val icon: ImageVector) {
    Wifi("Wi-Fi", "sieć telefonu", Icons.Rounded.Wifi),
    Mqtt("MQTT", "multimedia_remote/v1", Icons.Rounded.Repeat),
    Display("Ekran", "styl, jasność, wygaszanie", Icons.Rounded.Visibility),
    Sound("Dźwięk", "limit głośności Z906", Icons.Rounded.VolumeUp),
    Place("Pokój i czas", "nazwa pokoju, strefa czasowa", Icons.Rounded.Navigation),
    Keyboard("Klawiatura i gładzik", "tekst na żywo, czułość", Icons.Rounded.Keyboard),
    Diagnostics("Diagnostyka", "IP, łącze z domem, stan urządzeń", Icons.Rounded.List),
    Phone("Telefon", "ekran główny, aplikacje, Android", Icons.Rounded.PhoneAndroid),
    About("O urządzeniu", "wersja, restart, ustawienia fabryczne", Icons.Rounded.Info),
}

/**
 * Ustawienia 1:1 z płytki. W poziomie lista z lewej i szczegół z prawej (od razu Ekran),
 * w pionie lista, a strona na cały ekran.
 */
@Composable
fun SettingsScreen(state: PilotState, prefs: Preferences, repo: PilotRepository) {
    val landscape = LocalLandscape.current
    var pageName by rememberSaveable { mutableStateOf(if (landscape) Page.Display.name else null) }
    val page = pageName?.let { Page.valueOf(it) }
    val list: @Composable (Modifier) -> Unit = { modifier ->
        Column(modifier) {
            DeviceHeader(R.drawable.px_settings, "Pilot", "Ustawienia")
            PageList(state, prefs, page.takeIf { landscape }, Modifier.weight(1f)) { pageName = it.name }
        }
    }

    when {
        landscape -> Row(Modifier.fillMaxSize()) {
            list(Modifier.weight(0.45f).fillMaxHeight())
            Box(Modifier.weight(0.55f).fillMaxHeight()) {
                if (page != null) PageDetail(page, state, prefs, repo, back = null)
            }
        }
        page != null -> {
            BackHandler { pageName = null }
            PageDetail(page, state, prefs, repo) { pageName = null }
        }
        else -> list(Modifier.fillMaxSize())
    }
}

@Composable
private fun PageList(state: PilotState, prefs: Preferences, selected: Page?, modifier: Modifier, open: (Page) -> Unit) {
    val ip by rememberLocalIp()
    val isHome by rememberIsDefaultHome()
    Column(
        modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 24.u, vertical = 4.u),
        verticalArrangement = Arrangement.spacedBy(10.u),
    ) {
        for (p in Page.values()) {
            val (value, ok) = when (p) {
                Page.Wifi -> (if (ip != null) "połączono" else "brak sieci") to (ip != null)
                Page.Mqtt -> (if (state.link.connected) "połączono" else "brak") to state.link.connected
                Page.Display -> "${Options.themeLabels[prefs.theme]} · ${prefs.brightness}% · ${Options.timeoutLabels[prefs.timeout]}" to false
                Page.Sound -> (if (prefs.volumeLimit >= 100) "bez limitu" else "maks. ${prefs.volumeLimit}%") to false
                Page.Place -> prefs.roomName to false
                Page.Phone -> (if (isHome) "ekran główny" else "") to true
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
private fun PageDetail(page: Page, state: PilotState, prefs: Preferences, repo: PilotRepository, back: (() -> Unit)?) {
    Column(Modifier.fillMaxSize()) {
        PageHeader(page.title, onBack = back)
        Column(
            Modifier.fillMaxSize().imePadding().verticalScroll(rememberScrollState()).padding(horizontal = 24.u),
            verticalArrangement = Arrangement.spacedBy(16.u),
        ) {
            when (page) {
                Page.Wifi -> WifiPage()
                Page.Mqtt -> MqttPage(state, repo)
                Page.Display -> DisplayPage(prefs, repo::updatePreferences)
                Page.Sound -> SoundPage(prefs, repo::updatePreferences)
                Page.Place -> PlacePage(prefs, repo::updatePreferences)
                Page.Keyboard -> KeyboardPage(prefs, repo::updatePreferences)
                Page.Diagnostics -> DiagnosticsPage(state)
                Page.Phone -> PhonePage(prefs, repo::updatePreferences)
                Page.About -> AboutPage(repo)
            }
            Spacer(Modifier.height(24.u))
        }
    }
}
