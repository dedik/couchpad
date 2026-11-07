package app.couchpad.pilot.ui

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
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.verticalScroll
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AcUnit
import androidx.compose.material.icons.rounded.Alarm
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.Bedtime
import androidx.compose.material.icons.rounded.Block
import androidx.compose.material.icons.rounded.Celebration
import androidx.compose.material.icons.rounded.CenterFocusStrong
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material.icons.rounded.LightMode
import androidx.compose.material.icons.rounded.LocalFireDepartment
import androidx.compose.material.icons.rounded.MonitorHeart
import androidx.compose.material.icons.rounded.Nightlight
import androidx.compose.material.icons.rounded.PowerSettingsNew
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.Spa
import androidx.compose.material.icons.rounded.Tv
import androidx.compose.material.icons.rounded.WbIncandescent
import androidx.compose.material.icons.rounded.WbSunny
import androidx.compose.material.icons.rounded.WbTwilight
import androidx.compose.material.icons.rounded.Weekend
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.ClimateState
import app.couchpad.pilot.data.Light
import app.couchpad.pilot.data.LightMember
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import java.util.Locale

// --------------------------------------------------------------- dane stałe (screen_home.cpp)

private data class Room(val id: String, val label: String, val icon: Int)

private val Rooms = listOf(
    Room("salon", "Salon", R.drawable.px_room_salon),
    Room("kuchnia", "Kuchnia", R.drawable.px_room_kuchnia),
    Room("przedpokoj", "Przedpokój", R.drawable.px_room_przedpokoj),
    Room("biuro", "Biuro", R.drawable.px_room_biuro),
    Room("lazienka", "Łazienka", R.drawable.px_room_lazienka),
    Room("sypialnia", "Sypialnia", R.drawable.px_room_sypialnia),
)

/** Klimatyzacja z kontraktu (ADR 0009) wisi w salonie. */
private const val ClimateRoom = "salon"

private data class Effect(val label: String, val icon: ImageVector)

/** Efekty HA → polska nazwa i ikona (odpowiedniki ikon Lucide z firmware'u). */
private val Effects = mapOf(
    "Cozy" to Effect("Przytulnie", Icons.Rounded.Weekend),
    "Relax" to Effect("Relaks", Icons.Rounded.Spa),
    "TV time" to Effect("Seans TV", Icons.Rounded.Tv),
    "Candlelight" to Effect("Świeca", Icons.Rounded.LocalFireDepartment),
    "Warm white" to Effect("Ciepła biel", Icons.Rounded.WbIncandescent),
    "Golden white" to Effect("Złota biel", Icons.Rounded.WbSunny),
    "Daylight" to Effect("Dzienne", Icons.Rounded.LightMode),
    "Cool white" to Effect("Zimna biel", Icons.Rounded.AcUnit),
    "Focus" to Effect("Skupienie", Icons.Rounded.CenterFocusStrong),
    "Night light" to Effect("Nocne", Icons.Rounded.Nightlight),
    "Bedtime" to Effect("Do snu", Icons.Rounded.Bedtime),
    "Wake-up" to Effect("Pobudka", Icons.Rounded.WbTwilight),
    "Pulse" to Effect("Puls", Icons.Rounded.MonitorHeart),
    "Steampunk" to Effect("Steampunk", Icons.Rounded.Settings),
    "Diwali" to Effect("Diwali", Icons.Rounded.Celebration),
    "Alarm" to Effect("Alarm", Icons.Rounded.Alarm),
)

private fun effectName(id: String) = if (id.isEmpty()) "Bez efektu" else Effects[id]?.label ?: id
private fun effectIcon(id: String) = if (id.isEmpty()) Icons.Rounded.Block else Effects[id]?.icon ?: Icons.Rounded.AutoAwesome

private val HvacNames = mapOf(
    "off" to "Wyłączona", "cool" to "Chłodzenie", "heat" to "Grzanie", "heat_cool" to "Auto",
    "auto" to "Auto", "fan_only" to "Wentylator", "dry" to "Osuszanie",
)

private fun deg(v: Double) = String.format(Locale("pl"), "%.1f", v)

private fun bulbs(n: Int) = when {
    n == 1 -> "1 ŻARÓWKA"
    n % 10 in 2..4 && n % 100 !in 12..14 -> "$n ŻARÓWKI"
    else -> "$n ŻARÓWEK"
}

private fun lampImage(available: Boolean, on: Boolean, brightness: Int) = when {
    !available -> R.drawable.px_lamp_na
    !on -> R.drawable.px_lamp_off
    brightness >= 50 -> R.drawable.px_lamp_on
    else -> R.drawable.px_lamp_dim
}

// --------------------------------------------------------------- nawigacja

/** Poziomy z home-screen.md §1. Głębiej niż żarówka nie schodzimy. */
private sealed class Nav {
    object Rooms : Nav()
    data class RoomPage(val room: String) : Nav()
    data class LightPage(val room: String, val light: String) : Nav()
    data class BulbPage(val room: String, val light: String, val bulb: String) : Nav()
    data class Climate(val room: String) : Nav()

    val parent: Nav
        get() = when (this) {
            Rooms -> Rooms
            is RoomPage -> Rooms
            is LightPage -> RoomPage(room)
            is BulbPage -> LightPage(room, light)
            is Climate -> RoomPage(room)
        }
    val roomId: String?
        get() = when (this) {
            Rooms -> null
            is RoomPage -> room
            is LightPage -> room
            is BulbPage -> room
            is Climate -> room
        }
}

@Composable
fun HomeScreen(state: PilotState, repo: PilotRepository, onImmersive: (Boolean) -> Unit = {}) {
    var nav by remember { mutableStateOf<Nav>(Nav.Rooms) }
    val landscape = LocalLandscape.current
    var roomsDrawer by remember { mutableStateOf(false) }

    // W poziomie od pokoju w głąb treść zajmuje cały ekran, zakładki chowają się do szuflady.
    LaunchedEffect(nav, landscape) { onImmersive(landscape && nav != Nav.Rooms) }
    BackHandler(enabled = nav != Nav.Rooms) { nav = nav.parent }

    val switchRoom: (String) -> Unit = { id -> nav = Nav.RoomPage(id); roomsDrawer = false }
    val currentRoom = nav.roomId

    Box(Modifier.fillMaxSize()) {
        when (val n = nav) {
            Nav.Rooms -> RoomsGrid(state, repo, open = { nav = Nav.RoomPage(it) }, openClimate = { nav = Nav.Climate(ClimateRoom) })
            is Nav.RoomPage -> RoomPage(
                n.room, state, repo, { nav = n.parent }, switchRoom,
                openLight = { nav = Nav.LightPage(n.room, it) }, openClimate = { nav = Nav.Climate(n.room) },
            )
            is Nav.LightPage -> {
                val light = state.home.lights.firstOrNull { it.id == n.light }
                if (light == null) LaunchedEffect(Unit) { nav = n.parent }
                else LightPage(n.room, light, state, repo, { nav = n.parent }, switchRoom) { nav = Nav.BulbPage(n.room, n.light, it) }
            }
            is Nav.BulbPage -> {
                val light = state.home.lights.firstOrNull { it.id == n.light }
                val bulb = light?.members?.firstOrNull { it.id == n.bulb }
                if (light == null || bulb == null) LaunchedEffect(Unit) { nav = n.parent }
                else BulbPage(n.room, light, bulb, state, repo, { nav = n.parent }, switchRoom)
            }
            is Nav.Climate -> ClimateScreen(state, repo) { nav = n.parent }
        }
        if (landscape && currentRoom != null) {
            EdgeDrawer(left = false, open = roomsDrawer, onOpen = { roomsDrawer = it }, width = 132) {
                RoomStrip(currentRoom, vertical = true, onSelect = switchRoom)
            }
        }
    }
}

// --------------------------------------------------------------- Dom: siatka pokoi

@Composable
private fun RoomsGrid(state: PilotState, repo: PilotRepository, open: (String) -> Unit, openClimate: () -> Unit) {
    val landscape = LocalLandscape.current
    val climate = state.home.climate
    Column(Modifier.fillMaxSize()) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.weight(1f)) { Header(R.drawable.px_home, "Dom", repo.prefs.value.roomName) }
            if (landscape) {
                // Poziom: klimatyzacja jako pigułka w nagłówku.
                Row(
                    Modifier.padding(end = 24.u).height(46.u).clip(RoundedCornerShape(16.u)).background(C.surface)
                        .border(1.u, C.line, RoundedCornerShape(16.u))
                        .clickable(remember { MutableInteractionSource() }, null) { haptic(); openClimate() }
                        .padding(start = 16.u, end = 10.u),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    ClimateStripText(climate)
                    Icon(Icons.Rounded.ChevronRight, null, tint = C.faint, modifier = Modifier.size(22.u))
                }
            }
        }
        Column(Modifier.fillMaxSize().padding(horizontal = 24.u), verticalArrangement = Arrangement.spacedBy(12.u)) {
            if (!landscape) {
                // Pion: klimatyzacja jako wąski pasek nad siatką (56 px).
                Row(
                    Modifier.fillMaxWidth().height(56.u).clip(RoundedCornerShape(16.u)).background(C.surface)
                        .border(1.u, C.line, RoundedCornerShape(16.u))
                        .clickable(remember { MutableInteractionSource() }, null) { haptic(); openClimate() }
                        .padding(start = 16.u, end = 10.u),
                    verticalAlignment = Alignment.CenterVertically,
                ) {
                    Box(Modifier.weight(1f)) { ClimateStripText(climate) }
                    Icon(Icons.Rounded.ChevronRight, null, tint = C.faint, modifier = Modifier.size(22.u))
                }
            }
            for (row in Rooms.chunked(if (landscape) 3 else 2)) {
                Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(12.u)) {
                    for (r in row) RoomTile(r, state, repo, Modifier.weight(1f).fillMaxHeight()) { open(r.id) }
                }
            }
            Spacer(Modifier.height(4.u))
        }
    }
}

@Composable
private fun ClimateStripText(c: ClimateState) {
    Row(verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(7.u).clip(RoundedCornerShape(4.u)).background(if (c.available && c.hvacMode != "off") C.accent else C.faint))
        Spacer(Modifier.width(10.u))
        T("Klimatyzacja", 14, FontWeight.Bold)
        Spacer(Modifier.width(10.u))
        T(
            if (!c.available) "brak świeżego stanu"
            else (HvacNames[c.hvacMode] ?: c.hvacMode) +
                (if (c.hvacMode != "off" && c.target != null) " · ${deg(c.target)}°" else ""),
            13, color = C.dim,
        )
    }
}

/** Kafelek pomieszczenia (§4.1): ikona, lampka w prawym górnym rogu, nazwa i stan. */
@Composable
private fun RoomTile(room: Room, state: PilotState, repo: PilotRepository, modifier: Modifier, onClick: () -> Unit) {
    val main = state.home.lights.firstOrNull { it.room == room.id }
    val landscape = LocalLandscape.current
    val lit = main?.on == true && main.available
    Card(modifier, radius = 20, selected = lit, onClick = onClick) {
        Box(Modifier.fillMaxSize().padding(14.u)) {
            Pixel(room.icon, if (landscape) 72.u else 88.u, Modifier.align(Alignment.TopStart))
            if (main != null) {
                LampButton(main, "light.${main.id}" in state.pending, Modifier.align(Alignment.TopEnd)) {
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
                        main.effect.isNotEmpty() -> "${main.label} · ${effectName(main.effect)}"
                        else -> "${main.label} · ${main.brightness}%"
                    },
                    if (lit) C.accent else C.faint,
                    size = 12,
                )
            }
        }
    }
}

/** Lampka 64 × 64: cztery stany obrazka, pasek jasności 3 px u dołu, szybkie wł./wył. */
@Composable
private fun LampButton(light: Light, pending: Boolean, modifier: Modifier, onClick: () -> Unit) {
    val shape = RoundedCornerShape(16.u)
    val bg = when {
        !light.available || !light.on -> C.surface2
        light.brightness >= 50 -> C.accent.copy(alpha = 92 / 255f)
        else -> C.accent.copy(alpha = 46 / 255f)
    }
    Box(
        modifier.size(56.u).clip(shape).background(bg)
            .border(1.u, if (pending) C.accent else C.line, shape)
            .clickable(remember { MutableInteractionSource() }, null) { if (light.available) { haptic(); onClick() } },
        contentAlignment = Alignment.Center,
    ) {
        Pixel(lampImage(light.available, light.on, light.brightness), 44.u)
        if (light.on && light.available) {
            Box(Modifier.align(Alignment.BottomStart).fillMaxWidth(light.brightness / 100f).height(3.u).background(C.accent))
        }
    }
}

// --------------------------------------------------------------- pasek pomieszczeń

@Composable
private fun RoomStrip(current: String, vertical: Boolean, onSelect: (String) -> Unit) {
    val item: @Composable (Room, Modifier) -> Unit = { r, m ->
        val active = r.id == current
        Box(
            m.clip(RoundedCornerShape(16.u))
                .background(if (active) C.accent.copy(alpha = 0.16f) else Color.Transparent)
                .border(1.u, if (active) C.accent else Color.Transparent, RoundedCornerShape(16.u))
                .clickable(remember { MutableInteractionSource() }, null) { haptic(); onSelect(r.id) },
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

// --------------------------------------------------------------- nagłówek poziomów

@Composable
private fun LevelHeader(
    title: String,
    caption: String,
    back: () -> Unit,
    power: Boolean? = null,
    powerPending: Boolean = false,
    onPower: ((Boolean) -> Unit)? = null,
) {
    Row(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 10.u), verticalAlignment = Alignment.CenterVertically) {
        RoundButton(Icons.Rounded.ArrowBackIosNew, size = 44, onClick = back)
        Spacer(Modifier.width(14.u))
        Column(Modifier.weight(1f)) {
            T(title, 20, FontWeight.Bold)
            T(caption, 11, FontWeight.Medium, C.faint)
        }
        if (onPower != null) {
            val on = power == true
            RoundButton(
                Icons.Rounded.PowerSettingsNew, size = 44,
                tint = if (on) C.accentInk else C.accent,
                background = if (on) C.accent else C.accent.copy(alpha = 0.14f),
                border = if (powerPending || on) C.accent else C.line,
            ) { onPower(!on) }
        }
    }
}

// --------------------------------------------------------------- Pokój

@Composable
private fun RoomPage(
    roomId: String,
    state: PilotState,
    repo: PilotRepository,
    back: () -> Unit,
    switchRoom: (String) -> Unit,
    openLight: (String) -> Unit,
    openClimate: () -> Unit,
) {
    val room = Rooms.first { it.id == roomId }
    val landscape = LocalLandscape.current
    val lights = state.home.lights.filter { it.room == roomId }
    val climate = state.home.climate.takeIf { roomId == ClimateRoom && it.available }

    Column(Modifier.fillMaxSize()) {
        LevelHeader(room.label, "POMIESZCZENIE", back)
        if (!landscape) {
            RoomStrip(roomId, vertical = false, onSelect = switchRoom)
            Box(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 10.u).height(1.u).background(C.line))
        }
        T("URZĄDZENIA", 11, FontWeight.Medium, C.faint, Modifier.padding(horizontal = 24.u))
        Spacer(Modifier.height(10.u))
        val cards = mutableListOf<@Composable (Modifier) -> Unit>()
        for (light in lights) {
            cards += { m ->
                val members = light.members
                DeviceCard(
                    m, landscape,
                    well = { Pixel(lampImage(light.available, light.on, light.brightness), 60.u) },
                    wellLit = light.on && light.available,
                    name = light.label,
                    status = when {
                        !light.available -> "niedostępna"
                        !light.on -> "wyłączona"
                        light.effect.isNotEmpty() -> "efekt: ${effectName(light.effect)}"
                        light.dimmable -> "jasność ${light.brightness}%"
                        else -> "włączona"
                    },
                    detail = if (members.isNotEmpty()) "${members.count { it.on }} z ${members.size} żarówek" else null,
                    power = if (light.available) light.on else null,
                    pending = "light.${light.id}" in state.pending,
                    onPower = { repo.lightPower(light.id, it) },
                ) { openLight(light.id) }
            }
        }
        if (climate != null) {
            cards += { m ->
                DeviceCard(
                    m, landscape,
                    well = { Icon(Icons.Rounded.AcUnit, null, tint = C.accent, modifier = Modifier.size(40.u)) },
                    wellLit = false,
                    name = "Klimatyzacja",
                    status = HvacNames[climate.hvacMode] ?: climate.hvacMode,
                    detail = climate.current?.let { "w pokoju ${deg(it)}°" },
                    power = null, pending = false, onPower = null,
                    onClick = openClimate,
                )
            }
        }
        if (cards.isEmpty()) {
            Card(Modifier.padding(horizontal = 24.u).fillMaxWidth().height(96.u), radius = 20) {
                Column(Modifier.fillMaxSize().padding(18.u), verticalArrangement = Arrangement.Center) {
                    T("Brak encji", 17, FontWeight.Bold)
                    T(
                        if (state.home.known) "W tym pokoju nie ma jeszcze urządzeń w Home Assistancie." else "czekam na stan",
                        12, color = C.dim, maxLines = 2,
                    )
                }
            }
        } else if (landscape) {
            Row(
                Modifier.fillMaxSize().padding(start = 24.u, end = 40.u, bottom = 20.u),
                horizontalArrangement = Arrangement.spacedBy(12.u),
            ) {
                for (c in cards) c(Modifier.weight(1f).fillMaxHeight())
            }
        } else {
            Column(
                Modifier.fillMaxWidth().verticalScroll(rememberScrollState()).padding(horizontal = 24.u),
                verticalArrangement = Arrangement.spacedBy(12.u),
            ) {
                for (c in cards) c(Modifier.fillMaxWidth().height(124.u))
            }
        }
    }
}

/** Karta urządzenia w pokoju (§4.2): studzienka 80 px, nazwa, stan, szczegół, skrót, strzałka. */
@Composable
private fun DeviceCard(
    modifier: Modifier,
    landscape: Boolean,
    well: @Composable () -> Unit,
    wellLit: Boolean,
    name: String,
    status: String,
    detail: String?,
    power: Boolean?,
    pending: Boolean,
    onPower: ((Boolean) -> Unit)?,
    onClick: () -> Unit,
) {
    val wellBox: @Composable () -> Unit = {
        Box(
            Modifier.size(80.u).clip(RoundedCornerShape(16.u))
                .background(if (wellLit) C.accent.copy(alpha = 0.16f) else C.surface2)
                .border(1.u, if (wellLit) C.accent.copy(alpha = 0.6f) else C.line, RoundedCornerShape(16.u)),
            contentAlignment = Alignment.Center,
        ) { well() }
    }
    val texts: @Composable (Modifier) -> Unit = { m ->
        Column(m) {
            T(name, 19, FontWeight.Bold)
            Spacer(Modifier.height(4.u))
            T(status, 13, color = C.dim)
            if (detail != null) T(detail, 11, color = C.faint)
        }
    }
    val shortcut: @Composable () -> Unit = {
        if (power != null && onPower != null) {
            RoundButton(
                Icons.Rounded.PowerSettingsNew, size = 48,
                tint = if (power) C.accentInk else C.accent,
                background = if (power) C.accent else C.accent.copy(alpha = 0.14f),
                border = if (pending || power) C.accent else C.line,
            ) { onPower(!power) }
        }
    }
    Card(modifier, radius = 20, onClick = onClick) {
        if (landscape) {
            Box(Modifier.fillMaxSize().padding(18.u)) {
                Box(Modifier.align(Alignment.TopStart)) { wellBox() }
                Box(Modifier.align(Alignment.TopEnd)) { shortcut() }
                texts(Modifier.align(Alignment.BottomStart))
            }
        } else {
            Row(Modifier.fillMaxSize().padding(horizontal = 16.u), verticalAlignment = Alignment.CenterVertically) {
                wellBox()
                Spacer(Modifier.width(16.u))
                texts(Modifier.weight(1f))
                shortcut()
                Icon(Icons.Rounded.ChevronRight, null, tint = C.faint, modifier = Modifier.size(26.u))
            }
        }
    }
}

// --------------------------------------------------------------- Lampa

@Composable
private fun LightPage(
    roomId: String,
    light: Light,
    state: PilotState,
    repo: PilotRepository,
    back: () -> Unit,
    switchRoom: (String) -> Unit,
    openBulb: (String) -> Unit,
) {
    val room = Rooms.first { it.id == roomId }
    val landscape = LocalLandscape.current
    var effects by remember { mutableStateOf(false) }
    val pending = "light.${light.id}" in state.pending
    val caption = upperPl(room.label) + if (light.members.isNotEmpty()) " · ${bulbs(light.members.size)}" else ""

    val dial: @Composable (Modifier) -> Unit = { m ->
        Dial(
            value = if (light.on) light.brightness.toDouble() else null,
            min = 5.0, max = 100.0, step = 5.0,
            caption = when {
                !light.available -> "NIEDOSTĘPNA"
                !light.on -> "WYŁĄCZONA"
                light.effect.isNotEmpty() -> "EFEKT"
                else -> "JASNOŚĆ"
            },
            detail = if (light.on && light.effect.isNotEmpty()) effectName(light.effect) else null,
            unit = "%",
            format = { it.toInt().toString() },
            modifier = m,
            enabled = light.available && light.dimmable,
            pending = pending,
            minLabel = "5%", maxLabel = "100%",
        ) { repo.lightBrightness(light.id, it.toInt()) }
    }
    val tiles: @Composable () -> Unit = {
        if (light.minKelvin > 0) {
            KelvinSwatches(
                light.minKelvin, light.maxKelvin, light.kelvin,
                enabled = light.on && light.effect.isEmpty(),
                note = if (light.effect.isNotEmpty()) "barwa nie działa w efekcie" else null,
                pending = pending,
            ) { repo.lightKelvin(light.id, it) }
        }
        if (light.effects.isNotEmpty()) {
            Column {
                T(if (light.members.isNotEmpty()) "EFEKT · CAŁA LAMPA" else "EFEKT", 11, FontWeight.Medium, C.faint)
                Spacer(Modifier.height(8.u))
                OptionTile(
                    "EFEKT", effectName(light.effect),
                    Modifier.fillMaxWidth().height(64.u), active = light.effect.isNotEmpty(), pending = pending,
                    icon = {
                        Icon(
                            effectIcon(light.effect), null,
                            tint = if (light.effect.isNotEmpty()) C.accent else C.dim, modifier = Modifier.size(24.u),
                        )
                    },
                ) { effects = true }
            }
        }
        if (light.members.isNotEmpty()) {
            Column {
                T("ŻARÓWKI", 11, FontWeight.Medium, C.faint)
                Spacer(Modifier.height(8.u))
                Row(Modifier.fillMaxWidth().height(86.u), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                    for (m in light.members.take(4)) {
                        BulbTile(m, "light.${m.id}" in state.pending, Modifier.weight(1f).fillMaxHeight()) { openBulb(m.id) }
                    }
                }
            }
        }
    }

    Column(Modifier.fillMaxSize()) {
        LevelHeader(
            light.label, caption, back, power = light.on, powerPending = pending,
            onPower = if (light.available) { on -> repo.lightPower(light.id, on) } else null,
        )
        if (landscape) {
            Row(
                Modifier.fillMaxSize().padding(start = 24.u, end = 40.u, bottom = 16.u),
                horizontalArrangement = Arrangement.spacedBy(24.u),
            ) {
                Box(Modifier.width(320.u).fillMaxHeight(), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxHeight()) }
                Column(
                    Modifier.weight(1f).fillMaxHeight().verticalScroll(rememberScrollState()),
                    verticalArrangement = Arrangement.spacedBy(16.u),
                ) { tiles() }
            }
        } else {
            RoomStrip(roomId, vertical = false, onSelect = switchRoom)
            Box(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 8.u).height(1.u).background(C.line))
            Column(
                Modifier.fillMaxSize().verticalScroll(rememberScrollState()).padding(horizontal = 24.u),
                verticalArrangement = Arrangement.spacedBy(16.u),
            ) {
                Box(Modifier.fillMaxWidth().padding(horizontal = 36.u), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxWidth()) }
                tiles()
                Spacer(Modifier.height(12.u))
            }
        }
    }

    if (effects) {
        OptionDialog(
            "Efekt",
            listOf("" to "Brak") + light.effects.map { it to effectName(it) },
            light.effect,
            { effects = false },
            onSelect = { repo.lightEffect(light.id, it) },
            icon = ::effectIcon,
        )
    }
}

@Composable
private fun BulbTile(bulb: LightMember, pending: Boolean, modifier: Modifier, onClick: () -> Unit) {
    val active = bulb.on && bulb.available
    val shape = RoundedCornerShape(14.u)
    Column(
        modifier.clip(shape)
            .background(if (active) C.accent.copy(alpha = 0.16f) else C.surface2)
            .border(1.u, if (active || pending) C.accent.copy(alpha = if (pending && !active) 0.5f else 1f) else Color.Transparent, shape)
            .clickable(remember { MutableInteractionSource() }, null) { haptic(); onClick() }
            .padding(8.u),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Pixel(if (active) R.drawable.px_bulb_on else R.drawable.px_bulb_off, 30.u)
        Spacer(Modifier.height(4.u))
        T(upperPl(bulb.label), 11, FontWeight.Medium, if (active) C.accent else C.faint)
        T(
            if (!bulb.available) "—" else if (bulb.on) "${bulb.brightness}%" else "wył.",
            14, FontWeight.Bold, if (active) C.accent else C.text,
        )
    }
}

// --------------------------------------------------------------- Żarówka

@Composable
private fun BulbPage(
    roomId: String,
    light: Light,
    bulb: LightMember,
    state: PilotState,
    repo: PilotRepository,
    back: () -> Unit,
    switchRoom: (String) -> Unit,
) {
    val room = Rooms.first { it.id == roomId }
    val landscape = LocalLandscape.current
    val pending = "light.${bulb.id}" in state.pending
    val dial: @Composable (Modifier) -> Unit = { m ->
        Dial(
            value = if (bulb.on) bulb.brightness.toDouble() else null,
            min = 5.0, max = 100.0, step = 5.0,
            caption = if (!bulb.available) "NIEDOSTĘPNA" else if (bulb.on) "JASNOŚĆ" else "WYŁĄCZONA",
            detail = null, unit = "%", format = { it.toInt().toString() }, modifier = m,
            enabled = bulb.available, pending = pending, minLabel = "5%", maxLabel = "100%",
        ) { repo.lightBrightness(bulb.id, it.toInt()) }
    }
    val swatches: @Composable () -> Unit = {
        if (light.minKelvin > 0) {
            KelvinSwatches(light.minKelvin, light.maxKelvin, bulb.kelvin, enabled = bulb.on, note = null, pending = pending) {
                repo.lightKelvin(bulb.id, it)
            }
        }
    }
    Column(Modifier.fillMaxSize()) {
        LevelHeader(
            "Żarówka ${bulb.label}", "${upperPl(light.label)} · ${upperPl(room.label)}", back,
            power = bulb.on, powerPending = pending,
            onPower = if (bulb.available) { on -> repo.lightPower(bulb.id, on) } else null,
        )
        if (landscape) {
            Row(
                Modifier.fillMaxSize().padding(start = 24.u, end = 40.u, bottom = 16.u),
                horizontalArrangement = Arrangement.spacedBy(24.u),
            ) {
                Box(Modifier.width(320.u).fillMaxHeight(), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxHeight()) }
                Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.Center) { swatches() }
            }
        } else {
            RoomStrip(roomId, vertical = false, onSelect = switchRoom)
            Box(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 8.u).height(1.u).background(C.line))
            Column(Modifier.fillMaxSize().padding(horizontal = 24.u), verticalArrangement = Arrangement.spacedBy(16.u)) {
                Box(Modifier.fillMaxWidth().padding(horizontal = 36.u), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxWidth()) }
                swatches()
            }
        }
    }
}
