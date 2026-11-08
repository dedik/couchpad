package app.couchpad.pilot.ui.home

import androidx.compose.foundation.background
import androidx.compose.foundation.border
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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.ClimateState
import app.couchpad.pilot.data.Light
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.ui.climate.ClimateLabels
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.DeviceHeader
import app.couchpad.pilot.ui.components.StatusDot
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.oneDecimal
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/**
 * Dom (home-screen.md §2): siatka pokoi z lampką na kafelku i klimatyzacja — w pionie
 * pasek nad siatką, w poziomie pigułka w nagłówku.
 */
@Composable
internal fun RoomsGrid(state: PilotState, roomName: String, repo: PilotRepository, open: (String) -> Unit, openClimate: () -> Unit) {
    val landscape = LocalLandscape.current
    val climate = state.home.climate
    Column(Modifier.fillMaxSize()) {
        DeviceHeader(
            R.drawable.px_home, "Dom", roomName,
            extra = { if (landscape) ClimateStrip(climate, fill = false, Modifier.height(46.u), openClimate) },
        )
        Column(Modifier.fillMaxSize().padding(horizontal = 24.u), verticalArrangement = Arrangement.spacedBy(12.u)) {
            if (!landscape) ClimateStrip(climate, fill = true, Modifier.fillMaxWidth().height(56.u), openClimate)
            for (row in Rooms.chunked(if (landscape) 3 else 2)) {
                Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(12.u)) {
                    for (r in row) RoomTile(r, state, repo, Modifier.weight(1f).fillMaxHeight()) { open(r.id) }
                }
            }
            Spacer(Modifier.height(4.u))
        }
    }
}

/**
 * Klimatyzacja w skrócie: kropka, tryb i temperatura docelowa, strzałka do podstrony.
 * [fill] — pasek na całą szerokość (pion) ze strzałką przy prawej krawędzi.
 */
@Composable
private fun ClimateStrip(c: ClimateState, fill: Boolean, modifier: Modifier, onClick: () -> Unit) {
    val shape = RoundedCornerShape(16.u)
    Row(
        modifier.clip(shape).background(C.surface).border(1.u, C.line, shape).tap(onClick = onClick)
            .padding(start = 16.u, end = 10.u),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(if (fill) Modifier.weight(1f) else Modifier) {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Box(Modifier.size(7.u).clip(RoundedCornerShape(4.u)).background(if (c.available && c.on) C.accent else C.faint))
                Spacer(Modifier.width(10.u))
                T("Klimatyzacja", 14, FontWeight.Bold)
                Spacer(Modifier.width(10.u))
                T(
                    if (!c.available) "brak świeżego stanu"
                    else ClimateLabels.mode(c.hvacMode) + (if (c.on && c.target != null) " · ${oneDecimal(c.target)}°" else ""),
                    13, color = C.dim,
                )
            }
        }
        Icon(Icons.Rounded.ChevronRight, null, tint = C.faint, modifier = Modifier.size(22.u))
    }
}

/** Kafelek pomieszczenia (§4.1): ikona, lampka w prawym górnym rogu, nazwa i stan. */
@Composable
private fun RoomTile(room: Room, state: PilotState, repo: PilotRepository, modifier: Modifier, onClick: () -> Unit) {
    val main = state.home.lightsIn(room.id).firstOrNull()
    val landscape = LocalLandscape.current
    val lit = main?.lit == true
    Card(modifier, radius = 20, selected = lit, onClick = onClick) {
        Box(Modifier.fillMaxSize().padding(14.u)) {
            Pixel(room.icon, if (landscape) 72.u else 88.u, Modifier.align(Alignment.TopStart))
            if (main != null) {
                LampButton(main, state.isPending(Pending.Light.of(main.id)), Modifier.align(Alignment.TopEnd)) {
                    repo.lightPower(main.id, !main.on)
                }
            }
            Column(Modifier.align(Alignment.BottomStart)) {
                T(room.label, if (landscape) 22 else 24, FontWeight.Bold)
                Spacer(Modifier.height(4.u))
                StatusDot(
                    when {
                        !state.home.known -> "czekam na stan"
                        main == null -> "brak światła"
                        !main.available -> "${main.label} · niedostępna"
                        !main.on -> "${main.label} · wył."
                        main.effect.isNotEmpty() -> "${main.label} · ${Effects.name(main.effect)}"
                        else -> "${main.label} · ${main.brightness}%"
                    },
                    if (lit) C.accent else C.faint,
                    size = 12,
                )
            }
        }
    }
}

/** Lampka 56 × 56: cztery stany obrazka, pasek jasności 3 px u dołu, szybkie wł./wył. */
@Composable
private fun LampButton(light: Light, pending: Boolean, modifier: Modifier, onClick: () -> Unit) {
    val shape = RoundedCornerShape(16.u)
    val bg = when {
        !light.lit -> C.surface2
        light.brightness >= 50 -> C.accent.copy(alpha = 92 / 255f)
        else -> C.accent.copy(alpha = 46 / 255f)
    }
    Box(
        modifier.size(56.u).clip(shape).background(bg)
            .border(1.u, if (pending) C.accent else C.line, shape)
            .tap(enabled = light.available, onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Pixel(lampImage(light.available, light.on, light.brightness), 44.u)
        if (light.lit) {
            Box(Modifier.align(Alignment.BottomStart).fillMaxWidth(light.brightness / 100f).height(3.u).background(C.accent))
        }
    }
}

/** Pasek pomieszczeń: w pionie nad treścią pokoju, w poziomie w prawej szufladzie. */
@Composable
internal fun RoomStrip(current: String, vertical: Boolean, onSelect: (String) -> Unit) {
    val item: @Composable (Room, Modifier) -> Unit = { r, m ->
        val active = r.id == current
        val shape = RoundedCornerShape(16.u)
        Box(
            m.clip(shape)
                .background(if (active) C.accentSoft else Color.Transparent)
                .border(1.u, if (active) C.accent else Color.Transparent, shape)
                .tap { onSelect(r.id) },
            contentAlignment = Alignment.Center,
        ) { Pixel(r.icon, 48.u) }
    }
    if (vertical) {
        Column(Modifier.fillMaxSize().padding(10.u), verticalArrangement = Arrangement.spacedBy(6.u)) {
            for (r in Rooms) item(r, Modifier.fillMaxWidth().weight(1f))
        }
    } else {
        Row(Modifier.fillMaxWidth().height(68.u).padding(horizontal = 24.u), horizontalArrangement = Arrangement.spacedBy(6.u)) {
            for (r in Rooms) item(r, Modifier.weight(1f).fillMaxHeight())
        }
    }
}
