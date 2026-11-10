package app.couchpad.pilot.ui.tv

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
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.Bolt
import androidx.compose.material.icons.rounded.GridView
import androidx.compose.material.icons.rounded.Image
import androidx.compose.material.icons.rounded.Menu
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Option
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.ui.components.Button
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.Choice
import app.couchpad.pilot.ui.components.DPad
import app.couchpad.pilot.ui.components.DeviceHeader
import app.couchpad.pilot.ui.components.OptionDialog
import app.couchpad.pilot.ui.components.Rocker
import app.couchpad.pilot.ui.components.SelectorButton
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Logo
import app.couchpad.pilot.ui.theme.u

/** Skróty kanałów: nazwa dopasowywana w bridge'u bez wielkości liter i spacji. Także na zakładce Pilot. */
internal val Shortcuts = listOf(
    "Polsat" to R.drawable.logo_polsat,
    "TVN" to R.drawable.logo_tvn,
    "TVP1" to R.drawable.logo_tvp1,
    "TVP2" to R.drawable.logo_tvp2,
)

private enum class TvDialog { Source, Picture, Channels }

private fun List<Option>.choices() = map { Choice(it.id, it.label) }

/**
 * Telewizor przez Glasshouse (ADR 0008). Wake-on-LAN jest wyłączony, więc zasilanie
 * tylko pokazuje stan. VOL steruje Z906, jak na płytce.
 */
@Composable
fun TvScreen(state: PilotState, prefs: Preferences, repo: PilotRepository) {
    val tv = state.tv
    var dialog by remember { mutableStateOf<TvDialog?>(null) }

    val selectors: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(12.u)) {
            SelectorButton(tv.sourceLabel, Modifier.weight(1f), state.isPending(Pending.Tv.source), Icons.Rounded.Bolt) {
                dialog = TvDialog.Source
            }
            SelectorButton(tv.pictureLabel, Modifier.weight(1f), state.isPending(Pending.Tv.picture), Icons.Rounded.Image) {
                dialog = TvDialog.Picture
            }
        }
    }
    val header: @Composable (Boolean) -> Unit = { withSelectors ->
        DeviceHeader(
            R.drawable.px_tv, "LG OLED", prefs.roomName, power = tv.power, onPower = {},
            extra = { if (withSelectors) selectors(Modifier.width(420.u)) },
        )
    }
    val channel: @Composable (Modifier, Pair<String, Int>) -> Unit = { modifier, (name, logo) ->
        Card(modifier, radius = 16, background = C.logoPlate, onClick = { repo.tvChannel(name) }) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { Logo(logo, 40.u) }
        }
    }
    val allChannels: @Composable (Modifier) -> Unit = { modifier ->
        Card(modifier, radius = 16, onClick = { dialog = TvDialog.Channels }) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Icon(Icons.Rounded.GridView, null, tint = C.text, modifier = Modifier.width(24.u))
            }
        }
    }
    val pad: @Composable (Modifier, Modifier) -> Unit = { modifier, dpad ->
        Row(modifier, verticalAlignment = Alignment.CenterVertically) {
            // Jak na płytce: VOL na ekranie TV zmienia głośność Z906 o jeden poziom wzmacniacza.
            Rocker("VOL", Modifier.width(72.u).height(200.u)) { up -> repo.z906VolumeStep(if (up) 1 else -1) }
            Box(Modifier.weight(1f).padding(horizontal = 12.u), contentAlignment = Alignment.Center) {
                DPad(dpad) { repo.tvKey(it) }
            }
            Rocker("P", Modifier.width(72.u).height(200.u)) { up -> repo.tvChannelStep(up) }
        }
    }
    val bottom: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(12.u)) {
            Button("Cofnij", Modifier.weight(1f), icon = Icons.Rounded.ArrowBackIosNew) { repo.tvKey("back") }
            Button("Menu TV", Modifier.weight(1f), icon = Icons.Rounded.Menu) { repo.tvKey("settings") }
        }
    }

    Column(Modifier.fillMaxSize()) {
        if (LocalLandscape.current) {
            // Poziom: źródło i tryb obrazu w nagłówku, kanały 3 × 2 z lewej, krzyżak z prawej.
            header(true)
            Row(Modifier.fillMaxSize().padding(end = 16.u)) {
                Column(Modifier.weight(0.46f).fillMaxHeight().padding(top = 8.u)) {
                    Column(
                        Modifier.weight(1f).fillMaxWidth().padding(horizontal = 24.u),
                        verticalArrangement = Arrangement.spacedBy(10.u),
                    ) {
                        Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                            for (s in Shortcuts.take(3)) channel(Modifier.weight(1f).fillMaxHeight(), s)
                        }
                        Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                            channel(Modifier.weight(1f).fillMaxHeight(), Shortcuts[3])
                            allChannels(Modifier.weight(1f).fillMaxHeight())
                            Spacer(Modifier.weight(1f))
                        }
                    }
                    bottom(Modifier.fillMaxWidth().padding(24.u))
                }
                pad(Modifier.weight(0.54f).fillMaxHeight().padding(vertical = 16.u), Modifier.fillMaxHeight())
            }
        } else {
            header(false)
            selectors(Modifier.fillMaxWidth().padding(horizontal = 24.u))
            Spacer(Modifier.height(12.u))
            Row(Modifier.fillMaxWidth().height(64.u).padding(horizontal = 24.u), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                for (s in Shortcuts) channel(Modifier.weight(1f).fillMaxHeight(), s)
                allChannels(Modifier.weight(1f).fillMaxHeight())
            }
            pad(Modifier.fillMaxWidth().weight(1f).padding(horizontal = 16.u, vertical = 16.u), Modifier.fillMaxWidth())
            bottom(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 12.u))
        }
    }

    val close = { dialog = null }
    when (dialog) {
        TvDialog.Source -> OptionDialog("Źródło", tv.sources.choices(), tv.source, close, onSelect = repo::tvSource)
        TvDialog.Picture -> OptionDialog("Tryb obrazu", tv.pictureModes.choices(), tv.pictureMode, close, onSelect = repo::tvPictureMode)
        TvDialog.Channels -> OptionDialog(
            "Kanały", state.channels.map { Choice(it.id, "${it.number}  ${it.name}") }, null, close,
        ) { id -> state.channels.firstOrNull { it.id == id }?.let { repo.tvChannel(it.name, it.id) } }
        null -> Unit
    }
}
