package app.couchpad.pilot.ui.climate

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
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
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
import app.couchpad.pilot.ui.components.Segmented
import app.couchpad.pilot.ui.components.asChoices
import app.couchpad.pilot.ui.oneDecimal
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.LocalLandscape
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
                    Modes(c, repo)
                    Spacer(Modifier.height(8.u))
                    Caption("NAWIEW")
                    Airflow(c, state, Modifier.fillMaxWidth().height(80.u)) { dialog = it }
                }
            }
            else -> {
                Box(Modifier.fillMaxWidth().weight(1f).padding(horizontal = 56.u), contentAlignment = Alignment.Center) {
                    TemperatureDial(c, state, repo, Modifier.fillMaxWidth())
                }
                Column(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 16.u), verticalArrangement = Arrangement.spacedBy(12.u)) {
                    Modes(c, repo)
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

@Composable
private fun Modes(c: ClimateState, repo: PilotRepository) {
    val modes = c.hvacModes.filter { it != "off" }
    if (modes.isEmpty()) return
    Segmented(modes.map(ClimateLabels::mode), modes.indexOf(c.hvacMode)) { i -> repo.climateMode(modes[i]) }
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
    Card(Modifier.weight(1f).fillMaxHeight(), radius = 18, selected = pending, onClick = onClick) {
        Column(Modifier.fillMaxSize().padding(horizontal = 16.u), verticalArrangement = Arrangement.Center) {
            Caption(caption)
            Spacer(Modifier.height(6.u))
            T(value.ifEmpty { "—" }, 15, FontWeight.Bold)
        }
    }
}
