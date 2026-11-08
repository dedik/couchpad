package app.couchpad.pilot.ui.climate

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
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.data.ClimateState
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.Dial
import app.couchpad.pilot.ui.components.OptionDialog
import app.couchpad.pilot.ui.components.PageHeader
import app.couchpad.pilot.ui.components.PowerButton
import app.couchpad.pilot.ui.components.asChoices
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.oneDecimal
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

private enum class ClimateDialog { Fan, Swing, SwingHorizontal }

/**
 * Klimatyzacja (Dom → Klimatyzacja, ADR 0009): tarcza temperatury z zakresem i krokiem
 * z encji, tryby, moc nawiewu i żaluzje. W trybie „sam nawiew” temperatura nie działa,
 * więc tarcza jest wygaszona.
 */
@Composable
fun ClimateScreen(state: PilotState, repo: PilotRepository, back: () -> Unit) {
    val c = state.home.climate
    var dialog by remember { mutableStateOf<ClimateDialog?>(null) }

    Column(Modifier.fillMaxSize()) {
        PageHeader("Klimatyzacja", onBack = back) {
            PowerButton(c.on, pending = state.isPending(Pending.Climate.power)) { repo.climatePower(it) }
        }
        when {
            !c.available -> Box(Modifier.fillMaxSize().padding(32.u), contentAlignment = Alignment.Center) {
                T("Brak świeżego stanu klimatyzacji.", 15, color = C.dim)
            }
            LocalLandscape.current -> Row(Modifier.fillMaxSize()) {
                Box(Modifier.weight(0.45f).fillMaxHeight().padding(horizontal = 16.u, vertical = 8.u), contentAlignment = Alignment.Center) {
                    TemperatureDial(c, state, repo, Modifier.fillMaxHeight())
                }
                Column(
                    Modifier.weight(0.55f).fillMaxHeight().padding(end = 24.u, top = 12.u, bottom = 24.u),
                    verticalArrangement = Arrangement.spacedBy(10.u),
                ) {
                    Caption("TRYB")
                    Modes(c, state, repo)
                    Spacer(Modifier.height(8.u))
                    Caption("NAWIEW")
                    Airflow(c, state, Modifier.fillMaxWidth().height(68.u)) { dialog = it }
                }
            }
            else -> {
                Box(Modifier.fillMaxWidth().weight(1f).padding(horizontal = 56.u), contentAlignment = Alignment.Center) {
                    TemperatureDial(c, state, repo, Modifier.fillMaxWidth())
                }
                Column(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 16.u), verticalArrangement = Arrangement.spacedBy(12.u)) {
                    Modes(c, state, repo)
                    Airflow(c, state, Modifier.fillMaxWidth().height(88.u)) { dialog = it }
                }
            }
        }
    }

    val close = { dialog = null }
    when (dialog) {
        ClimateDialog.Fan ->
            OptionDialog("Moc nawiewu", c.fanModes.asChoices(ClimateLabels::fan), c.fanMode, close, onSelect = repo::climateFan)
        ClimateDialog.Swing ->
            OptionDialog("Żaluzje — pion", c.swingModes.asChoices(ClimateLabels::swing), c.swingMode, close, onSelect = repo::climateSwing)
        ClimateDialog.SwingHorizontal -> OptionDialog(
            "Żaluzje — poziom", c.swingHorizontalModes.asChoices(ClimateLabels::swing), c.swingHorizontalMode, close,
            onSelect = repo::climateSwingHorizontal,
        )
        null -> Unit
    }
}

/** Tarcza temperatury: zakres i krok z encji, „w pokoju” pod wartością. */
@Composable
private fun TemperatureDial(c: ClimateState, state: PilotState, repo: PilotRepository, modifier: Modifier) {
    Dial(
        value = c.target,
        min = c.min, max = c.max, step = c.step,
        caption = ClimateLabels.caption(c.hvacMode),
        detail = c.current?.let { "${oneDecimal(it)}° w pokoju" },
        unit = "°C",
        format = ::oneDecimal,
        modifier = modifier,
        enabled = c.on && c.hvacMode != "fan_only",
        pending = state.isPending(Pending.Climate.target),
        minLabel = "${oneDecimal(c.min)}°",
        maxLabel = "${oneDecimal(c.max)}°",
        onCommit = repo::climateTarget,
    )
}

/**
 * Tryby jako kafle z ikoną nad nazwą (makieta P1 z firmware'u): wybrany tryb na dużym kaflu
 * po lewej przez wszystkie rzędy, pozostałe po dwa w rzędzie. Wybrany ma miękkie
 * bursztynowe tło i obwódkę, reszta leży na surface-2 bez obwódki.
 */
@Composable
private fun Modes(c: ClimateState, state: PilotState, repo: PilotRepository) {
    val modes = c.hvacModes.filter { it != "off" }
    if (modes.isEmpty()) return
    val chosen = if (c.on) modes.indexOf(c.hvacMode) else -1
    val big = chosen.coerceAtLeast(0)
    val others = modes.indices.filter { it != big }
    val rows = ((others.size + 1) / 2).coerceAtLeast(1)
    val gap = 8.u
    val pending = state.isPending(Pending.Climate.mode)
    val tile: @Composable (Int, Boolean, Modifier) -> Unit = { i, large, modifier ->
        ModeTile(modes[i], large, selected = i == chosen, pending = pending && i == chosen, modifier) {
            repo.climateMode(modes[i])
        }
    }
    Row(
        Modifier.fillMaxWidth().height((72 * rows + 8 * (rows - 1)).u),
        horizontalArrangement = Arrangement.spacedBy(gap),
    ) {
        tile(big, true, Modifier.weight(6f).fillMaxHeight())
        Column(Modifier.weight(10f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(gap)) {
            others.chunked(2).forEach { pair ->
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    pair.forEach { tile(it, false, Modifier.weight(1f).fillMaxHeight()) }
                    if (pair.size == 1) Spacer(Modifier.weight(1f))
                }
            }
        }
    }
}

@Composable
private fun ModeTile(
    id: String,
    large: Boolean,
    selected: Boolean,
    pending: Boolean,
    modifier: Modifier,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(14.u)
    val ink = if (selected) C.accent else C.dim
    Column(
        modifier
            .clip(shape)
            .background(if (selected) C.accentSoft else C.surface2)
            .then(
                when {
                    pending -> Modifier.border(1.u, C.accentPending, shape)
                    selected -> Modifier.border(1.u, C.accent, shape)
                    else -> Modifier
                },
            )
            .tap(onClick = onClick),
        verticalArrangement = Arrangement.spacedBy(if (large) 10.u else 6.u, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Pixel(ClimateLabels.icon(id), if (large) 48.u else 24.u)
        T(
            ClimateLabels.mode(id),
            if (large) 15 else 13,
            if (large) FontWeight.Medium else FontWeight.Normal,
            ink,
            maxLines = 1,
        )
    }
}

/** Moc nawiewu i żaluzje — tylko te, które encja ma. */
@Composable
private fun Airflow(c: ClimateState, state: PilotState, modifier: Modifier, open: (ClimateDialog) -> Unit) {
    Row(modifier, horizontalArrangement = Arrangement.spacedBy(10.u)) {
        if (c.fanModes.isNotEmpty()) {
            OptionCard("MOC", ClimateLabels.fan(c.fanMode), state.isPending(Pending.Climate.fan)) { open(ClimateDialog.Fan) }
        }
        if (c.swingModes.isNotEmpty()) {
            OptionCard("PION", ClimateLabels.swing(c.swingMode), state.isPending(Pending.Climate.swing)) { open(ClimateDialog.Swing) }
        }
        if (c.swingHorizontalModes.isNotEmpty()) {
            OptionCard("POZIOM", ClimateLabels.swing(c.swingHorizontalMode), state.isPending(Pending.Climate.swingHorizontal)) {
                open(ClimateDialog.SwingHorizontal)
            }
        }
    }
}

@Composable
private fun androidx.compose.foundation.layout.RowScope.OptionCard(
    caption: String,
    value: String,
    pending: Boolean,
    onClick: () -> Unit,
) {
    if (LocalLandscape.current) {
        // Poziom jak w firmware (P1): pole na surface-2 bez obwódki, podpis i wartość na środku;
        // obwódka tylko przy oczekiwaniu.
        val shape = RoundedCornerShape(14.u)
        Column(
            Modifier.weight(1f).fillMaxHeight().clip(shape).background(C.surface2)
                .then(if (pending) Modifier.border(1.u, C.accentPending, shape) else Modifier)
                .tap(onClick = onClick),
            verticalArrangement = Arrangement.spacedBy(6.u, Alignment.CenterVertically),
            horizontalAlignment = Alignment.CenterHorizontally,
        ) {
            Caption(caption)
            T(value.ifEmpty { "—" }, 15, FontWeight.Bold)
        }
        return
    }
    Card(Modifier.weight(1f).fillMaxHeight(), radius = 18, selected = pending, onClick = onClick) {
        Column(Modifier.fillMaxSize().padding(horizontal = 16.u), verticalArrangement = Arrangement.Center) {
            Caption(caption)
            Spacer(Modifier.height(6.u))
            T(value.ifEmpty { "—" }, 15, FontWeight.Bold)
        }
    }
}
