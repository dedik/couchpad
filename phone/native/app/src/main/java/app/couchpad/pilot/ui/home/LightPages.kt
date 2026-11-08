package app.couchpad.pilot.ui.home

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
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
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Light
import app.couchpad.pilot.data.LightMember
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.ui.components.Choice
import app.couchpad.pilot.ui.components.Dial
import app.couchpad.pilot.ui.components.Divider
import app.couchpad.pilot.ui.components.KelvinSwatches
import app.couchpad.pilot.ui.components.OptionDialog
import app.couchpad.pilot.ui.components.OptionTile
import app.couchpad.pilot.ui.components.PageHeader
import app.couchpad.pilot.ui.components.PowerButton
import app.couchpad.pilot.ui.components.pendingBorder
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u
import app.couchpad.pilot.ui.upperPl

/**
 * Lampa (home-screen.md §4.3): tarcza jasności 5–100, próbki barwy, efekt dla całej lampy
 * i żarówki grupy.
 */
@Composable
internal fun LightPage(
    roomId: String,
    light: Light,
    state: PilotState,
    repo: PilotRepository,
    back: () -> Unit,
    switchRoom: (String) -> Unit,
    openBulb: (String) -> Unit,
) {
    var effects by remember { mutableStateOf(false) }
    val pending = state.isPending(Pending.Light.of(light.id))
    val caption = upperPl(room(roomId).label) + if (light.members.isNotEmpty()) " · ${bulbs(light.members.size)}" else ""

    DialPage(
        roomId, switchRoom, scrollSide = true,
        header = {
            PageHeader(light.label, caption, back) {
                if (light.available) PowerButton(light.on, pending = pending) { repo.lightPower(light.id, it) }
            }
        },
        dial = { m ->
            BrightnessDial(
                on = light.on, brightness = light.brightness, available = light.available, enabled = light.available && light.dimmable,
                caption = if (light.effect.isNotEmpty()) "EFEKT" else null,
                detail = if (light.on && light.effect.isNotEmpty()) Effects.name(light.effect) else null,
                pending = pending, modifier = m,
            ) { repo.lightBrightness(light.id, it) }
        },
    ) {
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
                Caption(if (light.members.isNotEmpty()) "EFEKT · CAŁA LAMPA" else "EFEKT")
                Spacer(Modifier.height(8.u))
                val active = light.effect.isNotEmpty()
                OptionTile(
                    "EFEKT", Effects.name(light.effect), Modifier.fillMaxWidth().height(64.u), active = active, pending = pending,
                    icon = { Icon(Effects.icon(light.effect), null, tint = if (active) C.accent else C.dim, modifier = Modifier.size(24.u)) },
                ) { effects = true }
            }
        }
        if (light.members.isNotEmpty()) {
            Column {
                Caption("ŻARÓWKI")
                Spacer(Modifier.height(8.u))
                Row(Modifier.fillMaxWidth().height(86.u), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                    for (m in light.members.take(4)) {
                        BulbTile(m, state.isPending(Pending.Light.of(m.id)), Modifier.weight(1f).fillMaxHeight()) { openBulb(m.id) }
                    }
                }
            }
        }
    }

    if (effects) {
        OptionDialog(
            "Efekt",
            listOf(Choice("", "Brak")) + light.effects.map { Choice(it, Effects.name(it)) },
            light.effect,
            onDismiss = { effects = false },
            icon = Effects::icon,
        ) { repo.lightEffect(light.id, it) }
    }
}

/** Żarówka grupy (§4.5): własna jasność i barwa w zakresie lampy. */
@Composable
internal fun BulbPage(
    roomId: String,
    light: Light,
    bulb: LightMember,
    state: PilotState,
    repo: PilotRepository,
    back: () -> Unit,
    switchRoom: (String) -> Unit,
) {
    val pending = state.isPending(Pending.Light.of(bulb.id))
    DialPage(
        roomId, switchRoom, scrollSide = false,
        header = {
            PageHeader("Żarówka ${bulb.label}", "${upperPl(light.label)} · ${upperPl(room(roomId).label)}", back) {
                if (bulb.available) PowerButton(bulb.on, pending = pending) { repo.lightPower(bulb.id, it) }
            }
        },
        dial = { m ->
            BrightnessDial(
                on = bulb.on, brightness = bulb.brightness, available = bulb.available, enabled = bulb.available,
                caption = null, detail = null, pending = pending, modifier = m,
            ) { repo.lightBrightness(bulb.id, it) }
        },
    ) {
        if (light.minKelvin > 0) {
            KelvinSwatches(light.minKelvin, light.maxKelvin, bulb.kelvin, enabled = bulb.on, note = null, pending = pending) {
                repo.lightKelvin(bulb.id, it)
            }
        }
    }
}

/**
 * Tarcza jasności 5–100 % co 5. Podpis: NIEDOSTĘPNA / WYŁĄCZONA / JASNOŚĆ, chyba że
 * [caption] mówi inaczej (EFEKT).
 */
@Composable
private fun BrightnessDial(
    on: Boolean,
    brightness: Int,
    available: Boolean,
    enabled: Boolean,
    caption: String?,
    detail: String?,
    pending: Boolean,
    modifier: Modifier,
    onCommit: (Int) -> Unit,
) {
    Dial(
        value = if (on) brightness.toDouble() else null,
        min = 5.0, max = 100.0, step = 5.0,
        caption = when {
            !available -> "NIEDOSTĘPNA"
            !on -> "WYŁĄCZONA"
            else -> caption ?: "JASNOŚĆ"
        },
        detail = detail,
        unit = "%",
        format = { it.toInt().toString() },
        modifier = modifier,
        enabled = enabled,
        pending = pending,
        minLabel = "5%", maxLabel = "100%",
    ) { onCommit(it.toInt()) }
}

/**
 * Układ strony z tarczą: w poziomie tarcza 320 px z lewej i treść z prawej, w pionie pasek
 * pokoi, tarcza i treść pod nią. [scrollSide] — treść przewijana; inaczej wyśrodkowana.
 */
@Composable
private fun DialPage(
    roomId: String,
    switchRoom: (String) -> Unit,
    scrollSide: Boolean,
    header: @Composable () -> Unit,
    dial: @Composable (Modifier) -> Unit,
    side: @Composable ColumnScope.() -> Unit,
) {
    Column(Modifier.fillMaxSize()) {
        header()
        if (LocalLandscape.current) {
            Row(
                Modifier.fillMaxSize().padding(start = 24.u, end = 40.u, bottom = 16.u),
                horizontalArrangement = Arrangement.spacedBy(24.u),
            ) {
                Box(Modifier.width(320.u).fillMaxHeight(), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxHeight()) }
                Column(
                    Modifier.weight(1f).fillMaxHeight().then(if (scrollSide) Modifier.verticalScroll(rememberScrollState()) else Modifier),
                    verticalArrangement = if (scrollSide) Arrangement.spacedBy(16.u) else Arrangement.Center,
                    content = side,
                )
            }
        } else {
            RoomStrip(roomId, vertical = false, onSelect = switchRoom)
            Divider(vertical = 8)
            Column(
                Modifier.fillMaxSize().then(if (scrollSide) Modifier.verticalScroll(rememberScrollState()) else Modifier)
                    .padding(horizontal = 24.u),
                verticalArrangement = Arrangement.spacedBy(16.u),
            ) {
                Box(Modifier.fillMaxWidth().padding(horizontal = 36.u), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxWidth()) }
                side()
                if (scrollSide) Spacer(Modifier.height(12.u))
            }
        }
    }
}

/** Kafel żarówki: obrazek, nazwa i jasność; świecąca w akcencie. */
@Composable
private fun BulbTile(bulb: LightMember, pending: Boolean, modifier: Modifier, onClick: () -> Unit) {
    val active = bulb.on && bulb.available
    val shape = RoundedCornerShape(14.u)
    Column(
        modifier.clip(shape)
            .background(if (active) C.accentSoft else C.surface2)
            .border(1.u, if (active || pending) pendingBorder(active, pending) else Color.Transparent, shape)
            .tap(onClick = onClick)
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
