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
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AcUnit
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.data.Light
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.ui.climate.ClimateLabels
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.Divider
import app.couchpad.pilot.ui.components.PageHeader
import app.couchpad.pilot.ui.components.PowerButton
import app.couchpad.pilot.ui.oneDecimal
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/** Karta urządzenia pokoju jako dane — ten sam opis w pionie i w poziomie. */
private data class Device(
    val name: String,
    val status: String,
    val detail: String?,
    val lit: Boolean,
    val well: @Composable () -> Unit,
    val power: Boolean? = null,
    val pending: Boolean = false,
    val onPower: ((Boolean) -> Unit)? = null,
    val onOpen: () -> Unit,
)

private fun lightDevice(light: Light, state: PilotState, repo: PilotRepository, open: (String) -> Unit) = Device(
    name = light.label,
    status = when {
        !light.available -> "niedostępna"
        !light.on -> "wyłączona"
        light.effect.isNotEmpty() -> "efekt: ${Effects.name(light.effect)}"
        light.dimmable -> "jasność ${light.brightness}%"
        else -> "włączona"
    },
    detail = light.members.takeIf { it.isNotEmpty() }?.let { m -> "${m.count { it.on }} z ${m.size} żarówek" },
    lit = light.lit,
    well = { Pixel(lampImage(light.available, light.on, light.brightness), 60.u) },
    power = if (light.available) light.on else null,
    pending = state.isPending(Pending.Light.of(light.id)),
    onPower = { repo.lightPower(light.id, it) },
    onOpen = { open(light.id) },
)

/**
 * Pokój (home-screen.md §3): pasek pokoi (pion), karty urządzeń — lampy i, w salonie,
 * klimatyzacja. Bez urządzeń karta „Brak encji”.
 */
@Composable
internal fun RoomPage(
    roomId: String,
    state: PilotState,
    repo: PilotRepository,
    back: () -> Unit,
    switchRoom: (String) -> Unit,
    openLight: (String) -> Unit,
    openClimate: () -> Unit,
) {
    val landscape = LocalLandscape.current
    val climate = state.home.climate.takeIf { roomId == ClimateRoom && it.available }
    val devices = state.home.lightsIn(roomId).map { lightDevice(it, state, repo, openLight) } +
        listOfNotNull(
            climate?.let { c ->
                Device(
                    name = "Klimatyzacja",
                    status = ClimateLabels.mode(c.hvacMode),
                    detail = c.current?.let { "w pokoju ${oneDecimal(it)}°" },
                    lit = false,
                    well = { Icon(Icons.Rounded.AcUnit, null, tint = C.accent, modifier = Modifier.size(40.u)) },
                    onOpen = openClimate,
                )
            }
        )

    Column(Modifier.fillMaxSize()) {
        PageHeader(room(roomId).label, "POMIESZCZENIE", back)
        if (!landscape) {
            RoomStrip(roomId, vertical = false, onSelect = switchRoom)
            Divider()
        }
        Caption("URZĄDZENIA", Modifier.padding(horizontal = 24.u))
        Spacer(Modifier.height(10.u))
        when {
            devices.isEmpty() -> NoDevices(state.home.known)
            landscape -> Row(
                Modifier.fillMaxSize().padding(start = 24.u, end = 40.u, bottom = 20.u),
                horizontalArrangement = Arrangement.spacedBy(12.u),
            ) {
                for (d in devices) DeviceCard(d, landscape = true, Modifier.weight(1f).fillMaxHeight())
            }
            else -> Column(
                Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 24.u),
                verticalArrangement = Arrangement.spacedBy(12.u),
            ) {
                for (d in devices) DeviceCard(d, landscape = false, Modifier.fillMaxWidth().height(124.u))
            }
        }
    }
}

@Composable
private fun NoDevices(known: Boolean) {
    Card(Modifier.padding(horizontal = 24.u).fillMaxWidth().height(96.u), radius = 20) {
        Column(Modifier.fillMaxSize().padding(18.u), verticalArrangement = Arrangement.Center) {
            T("Brak encji", 17, FontWeight.Bold)
            T(if (known) "W tym pokoju nie ma jeszcze urządzeń w Home Assistancie." else "czekam na stan", 12, color = C.dim, maxLines = 2)
        }
    }
}

/**
 * Karta urządzenia w pokoju (§4.2): studzienka 80 px, nazwa, stan, szczegół, skrót
 * zasilania. W pionie wiersz ze strzałką, w poziomie wysoka karta.
 */
@Composable
private fun DeviceCard(d: Device, landscape: Boolean, modifier: Modifier) {
    val well: @Composable () -> Unit = {
        val shape = RoundedCornerShape(16.u)
        Box(
            Modifier.size(80.u).clip(shape)
                .background(if (d.lit) C.accentSoft else C.surface2)
                .border(1.u, if (d.lit) C.accent.copy(alpha = 0.6f) else C.line, shape),
            contentAlignment = Alignment.Center,
        ) { d.well() }
    }
    val texts: @Composable (Modifier) -> Unit = { m ->
        Column(m) {
            T(d.name, 19, FontWeight.Bold)
            Spacer(Modifier.height(4.u))
            T(d.status, 13, color = C.dim)
            if (d.detail != null) T(d.detail, 11, color = C.faint)
        }
    }
    val shortcut: @Composable () -> Unit = {
        if (d.power != null && d.onPower != null) PowerButton(d.power, size = 48, pending = d.pending, onToggle = d.onPower)
    }
    Card(modifier, radius = 20, onClick = d.onOpen) {
        if (landscape) {
            Box(Modifier.fillMaxSize().padding(18.u)) {
                Box(Modifier.align(Alignment.TopStart)) { well() }
                Box(Modifier.align(Alignment.TopEnd)) { shortcut() }
                texts(Modifier.align(Alignment.BottomStart))
            }
        } else {
            Row(Modifier.fillMaxSize().padding(horizontal = 16.u), verticalAlignment = Alignment.CenterVertically) {
                well()
                Spacer(Modifier.width(16.u))
                texts(Modifier.weight(1f))
                shortcut()
                Icon(Icons.Rounded.ChevronRight, null, tint = C.faint, modifier = Modifier.size(26.u))
            }
        }
    }
}
