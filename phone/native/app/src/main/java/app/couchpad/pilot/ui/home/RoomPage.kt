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
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Light
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Radiator
import app.couchpad.pilot.data.RoomReading
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
    /** Studzienka „grzeje” (grzejnik z otwartym zaworem): ciepła poświata zamiast akcentu. */
    val heat: Boolean = false,
    val power: Boolean? = null,
    val pending: Boolean = false,
    /** Oczekiwanie na całej karcie (urządzenie bez skrótu zasilania). */
    val cardPending: Boolean = false,
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
 * Pokój (home-screen.md §3): pasek pokoi (pion), karty urządzeń — lampy, w salonie
 * klimatyzacja i grzejnik, jeśli pokój go ma (ADR 0018). Bez urządzeń karta „Brak encji”.
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
    openRadiator: () -> Unit,
) {
    val landscape = LocalLandscape.current
    // Karta „W pokoju” (ADR 0017): czujnik pokoju z wiadomości pogody, tak samo świeży jak ona.
    val reading = state.weather.takeIf { it.current(System.currentTimeMillis()) }?.room(roomId)
    val climate = state.home.climate.takeIf { roomId == ClimateRoom && it.available }
    val devices = state.home.lightsIn(roomId).map { lightDevice(it, state, repo, openLight) } +
        listOfNotNull(
            climate?.let { c ->
                Device(
                    name = "Klimatyzacja",
                    status = ClimateLabels.mode(c.hvacMode),
                    detail = c.current?.let { "w pokoju ${oneDecimal(it)}°" },
                    lit = false,
                    well = { Pixel(R.drawable.px_dev_aircon, 48.u) },
                    onOpen = openClimate,
                )
            },
            state.home.radiatorIn(roomId)?.let { r -> radiatorDevice(r, reading?.temperature, state, openRadiator) },
        )
    // Salon w poziomie: „W pokoju” i trzy urządzenia — węższa kolumna „W pokoju”.
    val compact = landscape && devices.size + (if (reading != null) 1 else 0) >= 4

    Column(Modifier.fillMaxSize()) {
        PageHeader(room(roomId).label, "POMIESZCZENIE", back)
        if (!landscape) {
            RoomStrip(roomId, vertical = false, onSelect = switchRoom)
            Divider()
        }
        if (landscape) {
            Caption(if (reading != null) "W POKOJU · URZĄDZENIA" else "URZĄDZENIA", Modifier.padding(horizontal = 24.u))
            Spacer(Modifier.height(10.u))
            Row(
                Modifier.fillMaxSize().padding(start = 24.u, end = 40.u, bottom = 20.u),
                horizontalArrangement = Arrangement.spacedBy(if (compact) 10.u else 12.u),
            ) {
                if (reading != null) {
                    RoomClimateCard(reading, landscape = true, Modifier.width(if (compact) 160.u else 210.u).fillMaxHeight(), compact)
                }
                if (devices.isEmpty()) Box(Modifier.weight(1f)) { NoDevices(state.home.known, padded = false) }
                for (d in devices) DeviceCard(d, landscape = true, Modifier.weight(1f).fillMaxHeight(), compact)
            }
        } else {
            Column(
                Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 24.u),
            ) {
                if (reading != null) {
                    Caption("W POKOJU")
                    Spacer(Modifier.height(10.u))
                    RoomClimateCard(reading, landscape = false, Modifier.fillMaxWidth())
                    Spacer(Modifier.height(20.u))
                }
                Caption("URZĄDZENIA")
                Spacer(Modifier.height(10.u))
                if (devices.isEmpty()) NoDevices(state.home.known, padded = false)
                Column(verticalArrangement = Arrangement.spacedBy(12.u)) {
                    for (d in devices) DeviceCard(d, landscape = false, Modifier.fillMaxWidth().height(124.u))
                }
            }
        }
    }
}

/**
 * Karta grzejnika (ADR 0018): stan i nastawa, dopisek z temperaturą czujnika pokoju [roomTemp]
 * (nigdy głowicy) i stanem zaworu. Bez skrótu zasilania — strzałka otwiera ekran grzejnika.
 */
private fun radiatorDevice(r: Radiator, roomTemp: Double?, state: PilotState, open: () -> Unit) = Device(
    name = "Grzejnik",
    status = RadiatorLabels.status(r),
    detail = RadiatorLabels.note(r, roomTemp),
    lit = false,
    heat = r.heats,
    well = { RadiatorWell(r) },
    cardPending = state.isPendingAny(Pending.Radiator.any(r.id)),
    onOpen = open,
)

/** Poświata studzienki grzejącego grzejnika (makieta: radial-gradient z #ff6b4a). */
private val HeatGlow = Brush.radialGradient(listOf(Color(0x6BFF6B4A), Color(0x0FFF6B4A)))

/**
 * Karta „W pokoju”: temperatura i wilgotność z czujnika pokoju, każda z ikoną pixel art
 * (48 px) i dużą liczbą, bez oceny słowem. Pion: obok siebie; poziom: wąska kolumna, jedna nad drugą.
 */
@Composable
private fun RoomClimateCard(r: RoomReading, landscape: Boolean, modifier: Modifier, compact: Boolean = false) {
    val cells = listOfNotNull(
        r.temperature?.let { t -> Cell(R.drawable.px_sensor_temp, oneDecimal(t), "°C") },
        r.humidity?.let { h -> Cell(R.drawable.px_sensor_humidity, "$h", "%") },
    )
    Card(modifier, radius = 20) {
        if (landscape) {
            Column(Modifier.fillMaxSize().padding(horizontal = if (compact) 14.u else 20.u, vertical = if (compact) 16.u else 20.u)) {
                cells.forEachIndexed { i, cell ->
                    if (i > 0) Box(Modifier.fillMaxWidth().height(1.u).background(C.line))
                    Reading(cell, Modifier.weight(1f), compact)
                }
            }
        } else {
            Row(Modifier.fillMaxWidth().padding(horizontal = 18.u, vertical = 16.u), verticalAlignment = Alignment.CenterVertically) {
                cells.forEachIndexed { i, cell ->
                    if (i > 0) {
                        Spacer(Modifier.width(16.u))
                        Box(Modifier.width(1.u).height(64.u).background(C.line))
                        Spacer(Modifier.width(16.u))
                    }
                    Reading(cell, Modifier.weight(1f))
                }
            }
        }
    }
}

private data class Cell(val icon: Int, val value: String, val unit: String)

@Composable
private fun Reading(cell: Cell, modifier: Modifier, compact: Boolean = false) {
    val (icon, value, unit) = cell
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        Pixel(icon, if (compact) 32.u else 48.u)
        Spacer(Modifier.width(if (compact) 8.u else 12.u))
        Column {
            Row(verticalAlignment = Alignment.Bottom) {
                T(value, if (compact) 32 else 46, FontWeight.Bold)
                Spacer(Modifier.width(3.u))
                T(unit, if (compact) 13 else 15, FontWeight.Medium, C.dim, Modifier.padding(bottom = if (compact) 5.u else 8.u))
            }
        }
    }
}

@Composable
private fun NoDevices(known: Boolean, padded: Boolean = true) {
    Card(Modifier.padding(horizontal = if (padded) 24.u else 0.u).fillMaxWidth().height(96.u), radius = 20) {
        Column(Modifier.fillMaxSize().padding(18.u), verticalArrangement = Arrangement.Center) {
            T("Brak encji", 17, FontWeight.Bold)
            T(if (known) "W tym pokoju nie ma jeszcze urządzeń w Home Assistancie." else "czekam na stan", 12, color = C.dim, maxLines = 2)
        }
    }
}

/**
 * Karta urządzenia w pokoju (§4.2): studzienka 80 px, nazwa, stan, szczegół, skrót
 * zasilania. W pionie wiersz ze strzałką, w poziomie wysoka karta; [compact] (trzy
 * urządzenia obok „W pokoju”) — mniejsze marginesy, ten sam układ.
 */
@Composable
private fun DeviceCard(d: Device, landscape: Boolean, modifier: Modifier, compact: Boolean = false) {
    val well: @Composable () -> Unit = {
        val shape = RoundedCornerShape(16.u)
        Box(
            Modifier.size(80.u).clip(shape)
                .then(
                    when {
                        d.heat -> Modifier.background(HeatGlow).border(1.u, HeatInk.copy(alpha = 0.55f), shape)
                        d.lit -> Modifier.background(C.accentSoft).border(1.u, C.accent.copy(alpha = 0.6f), shape)
                        else -> Modifier.background(C.surface2).border(1.u, C.line, shape)
                    },
                ),
            contentAlignment = Alignment.Center,
        ) { d.well() }
    }
    val texts: @Composable (Modifier) -> Unit = { m ->
        Column(m) {
            T(d.name, 19, FontWeight.Bold)
            Spacer(Modifier.height(4.u))
            T(d.status, 13, color = C.dim)
            if (d.detail != null) T(d.detail, 11, color = C.faint, maxLines = 2)
        }
    }
    val shortcut: @Composable () -> Unit = {
        if (d.power != null && d.onPower != null) PowerButton(d.power, size = 48, pending = d.pending, onToggle = d.onPower)
    }
    Card(modifier, radius = 20, pending = d.cardPending, onClick = d.onOpen) {
        if (landscape) {
            // [compact] tylko zmniejsza marginesy — układ jak zawsze: zasilanie w prawym górnym
            // rogu obok studzienki, nazwa i stan przy dolnej krawędzi.
            Box(Modifier.fillMaxSize().padding(if (compact) 14.u else 18.u)) {
                Box(Modifier.align(Alignment.TopStart)) { well() }
                Box(Modifier.align(Alignment.TopEnd)) { shortcut() }
                texts(Modifier.align(Alignment.BottomStart).padding(end = 28.u))
                Icon(
                    Icons.Rounded.ChevronRight, null, tint = C.faint,
                    modifier = Modifier.align(Alignment.BottomEnd).size(26.u),
                )
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
