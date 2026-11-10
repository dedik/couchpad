package app.couchpad.pilot.ui.start

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.LinkPhase
import app.couchpad.pilot.data.LinkState
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.data.zoneId
import app.couchpad.pilot.ui.components.Button
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.RoundButton
import app.couchpad.pilot.ui.components.StatusDot
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.formatIn
import app.couchpad.pilot.ui.oneDecimal
import app.couchpad.pilot.ui.shell.Screen
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u
import java.util.Date

/** Kafelek urządzenia na ekranie startowym. */
private data class DeviceTile(val icon: Int, val title: String, val status: String, val ok: Boolean, val screen: Screen)

private fun deviceTiles(s: PilotState): List<DeviceTile> {
    val fresh = s.link.fresh
    return listOf(
        DeviceTile(R.drawable.px_tv, "Telewizor", tvStatus(s), s.tv.available && fresh, Screen.Tv),
        DeviceTile(R.drawable.px_appletv, "Apple TV", appleStatus(s), s.home.appleTv.available && fresh, Screen.AppleTv),
        DeviceTile(R.drawable.px_audio, "Głośniki", audioStatus(s), s.z906.available && fresh, Screen.Audio),
        DeviceTile(R.drawable.px_home, "Dom", homeStatus(s), s.home.known, Screen.Home),
    )
}

/** Ekran startowy: zegar, pogoda, cztery urządzenia i stan łącza z domem. */
@Composable
fun StartScreen(state: PilotState, prefs: Preferences, now: Long, onRetry: () -> Unit, open: (Screen) -> Unit) {
    val date = Date(now)
    val tiles = deviceTiles(state)
    val dateLine = formatIn("EEEE, d MMMM", prefs.zoneId, date) + " · " + prefs.roomName.uppercase()
    val clock = formatIn("H:mm", prefs.zoneId, date)
    val weather = state.weather.takeIf { it.current(now) }

    if (LocalLandscape.current) {
        // Poziom: zegar, pogoda i stan z lewej, kafelki 2 × 2 z prawej (ADR 0013).
        Row(Modifier.fillMaxSize().padding(20.u), horizontalArrangement = Arrangement.spacedBy(16.u)) {
            Column(Modifier.weight(0.42f).fillMaxHeight()) {
                Column(
                    Modifier.weight(1f).fillMaxWidth(),
                    horizontalAlignment = Alignment.CenterHorizontally,
                    verticalArrangement = Arrangement.Center,
                ) {
                    T(clock, 80, FontWeight.Bold)
                    T(dateLine, 13, color = C.dim)
                }
                if (weather != null) WeatherTiles(weather)
                Spacer(Modifier.height(14.u))
                LinkRow(state.link, compact = true, onRetry) { open(Screen.Settings) }
            }
            TileGrid(tiles, Modifier.weight(0.58f).fillMaxHeight(), iconSize = 72, open)
        }
    } else {
        Column(Modifier.fillMaxSize().padding(horizontal = 24.u)) {
            Spacer(Modifier.height(20.u))
            Row(verticalAlignment = Alignment.Top) {
                Column(Modifier.weight(1f)) {
                    T(clock, 72, FontWeight.Bold)
                    T(dateLine, 13, color = C.dim)
                }
                if (weather != null) WeatherBlock(weather)
            }
            Spacer(Modifier.height(24.u))
            TileGrid(tiles, Modifier.weight(1f), iconSize = 96, open)
            Box(Modifier.fillMaxWidth().height(84.u), contentAlignment = Alignment.CenterStart) {
                LinkRow(state.link, compact = false, onRetry) { open(Screen.Settings) }
            }
            Spacer(Modifier.height(4.u))
        }
    }
}

/**
 * Kafelki 2 × 2 i na ich skrzyżowaniu duży okrągły przycisk zakładki Pilot (tylko aplikacja,
 * makieta docs/mockups/start-remote.html). Górny rząd ma tytuł u góry, żeby przycisk nie
 * zasłaniał napisów.
 */
@Composable
private fun TileGrid(tiles: List<DeviceTile>, modifier: Modifier, iconSize: Int, open: (Screen) -> Unit) {
    Box(modifier, contentAlignment = Alignment.Center) {
        Column(Modifier.fillMaxSize(), verticalArrangement = Arrangement.spacedBy(12.u)) {
            tiles.chunked(2).forEachIndexed { index, row ->
                Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(12.u)) {
                    for (tile in row) DeviceTileCard(tile, Modifier.weight(1f), iconSize, titleOnTop = index == 0) { open(tile.screen) }
                }
            }
        }
        RemoteHub(if (iconSize < 96) 147 else 176) { open(Screen.Remote) }
    }
}

/**
 * Przycisk Pilota: koło z obrysem w akcencie i ikoną pilota. Obwódka w kolorze tła wycina łuk
 * w narożnikach kafelków, więc przycisk wygląda na osadzony między nimi.
 */
@Composable
private fun RemoteHub(size: Int, onClick: () -> Unit) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()
    Box(Modifier.size((size + 24).u).clip(CircleShape).background(C.bg), contentAlignment = Alignment.Center) {
        Box(
            Modifier.size(size.u).clip(CircleShape)
                .background(if (pressed) C.surface3 else C.surface2)
                .border(2.u, C.accent, CircleShape)
                .tap(interaction = interaction, onClick = onClick),
            contentAlignment = Alignment.Center,
        ) { Pixel(R.drawable.px_remote, (size * 0.48f).u) }
    }
}

@Composable
private fun DeviceTileCard(tile: DeviceTile, modifier: Modifier, iconSize: Int, titleOnTop: Boolean, onClick: () -> Unit) {
    val compact = iconSize < 96
    Card(modifier.fillMaxHeight(), radius = 24, onClick = onClick) {
        Column(Modifier.fillMaxSize().padding(if (compact) 14.u else 20.u)) {
            val text: @Composable () -> Unit = {
                T(tile.title, if (compact) 22 else 26, FontWeight.Bold)
                Spacer(Modifier.height(8.u))
                StatusDot(tile.status, if (tile.ok) C.accent else C.danger, size = 13)
            }
            if (titleOnTop) text()
            Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                Pixel(tile.icon, iconSize.u)
            }
            if (!titleOnTop) text()
        }
    }
}

/** Stan łącza z domem, „Ponów” po błędzie i wejście do ustawień. */
@Composable
private fun LinkRow(link: LinkState, compact: Boolean, onRetry: () -> Unit, onSettings: () -> Unit) {
    val ready = link.connected && link.fresh
    val failed = link.phase == LinkPhase.Error
    val buttonSize = if (compact) 44 else 48
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        StatusDot(
            when {
                ready -> "Połączono z domem"
                link.phase == LinkPhase.Connecting -> "Łączenie z domem…"
                link.connected -> "Czekam na stan z domu…"
                else -> link.error ?: "Brak połączenia z domem"
            },
            if (ready) C.ok else if (failed) C.danger else C.accent,
            Modifier.weight(1f),
        )
        if (failed) {
            if (compact) {
                RoundButton(Icons.Rounded.Refresh, size = buttonSize, tint = C.accentInk, background = C.accent, onClick = onRetry)
                Spacer(Modifier.width(8.u))
            } else {
                Button("Ponów", Modifier.width(120.u), icon = Icons.Rounded.Refresh, accent = true, height = buttonSize, onClick = onRetry)
                Spacer(Modifier.width(10.u))
            }
        }
        RoundButton(Icons.Rounded.Settings, size = buttonSize, onClick = onSettings)
    }
}

private fun tvStatus(s: PilotState): String {
    val tv = s.tv
    if (!tv.known || !tv.available) return "brak połączenia"
    if (!tv.power) return "wyłączony"
    return listOf(tv.sourceLabel, tv.pictureLabel).filter { it.isNotEmpty() }.joinToString(" · ").ifEmpty { "włączony" }
}

private fun appleStatus(s: PilotState): String {
    val a = s.home.appleTv
    if (!s.home.known || !a.available) return "brak połączenia"
    if (!a.power) return "uśpione"
    return a.appLabel ?: "ekran główny"
}

private fun audioStatus(s: PilotState): String {
    val z = s.z906
    if (!z.known || !z.available) return "brak połączenia"
    if (z.power == false) return "wyłączone"
    return listOfNotNull(z.volume?.let { "głośność $it" }, z.source.ifEmpty { null }).joinToString(" · ")
}

private fun homeStatus(s: PilotState): String {
    if (!s.home.known) return "brak połączenia"
    val lit = s.home.lights.count { it.on }
    val climate = s.home.climate
    val parts = mutableListOf<String>()
    if (climate.available) {
        parts += if (!climate.on) "klima wyłączona" else "klima ${climate.target?.let { oneDecimal(it) + "°" } ?: ""}"
    }
    parts += if (lit == 0) "światła zgaszone" else "światła: $lit"
    return parts.joinToString(" · ")
}
