package app.couchpad.pilot.ui

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
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState

/** Skróty kanałów: nazwa dopasowywana w bridge'u bez wielkości liter i spacji. */
private val Shortcuts = listOf(
    "Polsat" to R.drawable.logo_polsat,
    "TVN" to R.drawable.logo_tvn,
    "TVP1" to R.drawable.logo_tvp1,
    "TVP2" to R.drawable.logo_tvp2,
)

@Composable
fun TvScreen(state: PilotState, repo: PilotRepository) {
    val tv = state.tv
    var dialog by remember { mutableStateOf<String?>(null) }
    val landscape = LocalLandscape.current

    val selectors: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(12.u)) {
            SelectorButton(tv.sourceLabel, Modifier.weight(1f), "tv.source" in state.pending, Icons.Rounded.Bolt) { dialog = "source" }
            SelectorButton(tv.pictureLabel, Modifier.weight(1f), "tv.picture" in state.pending, Icons.Rounded.Image) { dialog = "picture" }
        }
    }
    val channelTile: @Composable (Modifier, String, Int) -> Unit = { modifier, name, logo ->
        Card(modifier, radius = 16, onClick = { repo.tvChannel(name) }) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { Logo(logo, 40.u) }
        }
    }
    val channelsTile: @Composable (Modifier) -> Unit = { modifier ->
        Card(modifier, radius = 16, onClick = { dialog = "channels" }) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Icon(Icons.Rounded.GridView, null, tint = C.text, modifier = Modifier.width(24.u))
            }
        }
    }
    val volume: @Composable (Modifier) -> Unit = { modifier ->
        Rocker("VOL", modifier) { up ->
            // Jak na płytce: VOL na ekranie TV zmienia głośność Z906 o jeden poziom wzmacniacza.
            state.z906.volume?.let { v -> repo.z906Volume(stepVolume(v, if (up) 1 else -1, repo.prefs.value.volumeLimit)) }
        }
    }
    val bottom: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(12.u)) {
            Button("Cofnij", Modifier.weight(1f), icon = Icons.Rounded.ArrowBackIosNew) { repo.tvKey("back") }
            Button("Menu TV", Modifier.weight(1f), icon = Icons.Rounded.Menu) { repo.tvKey("settings") }
        }
    }

    if (landscape) {
        // Poziom: nagłówek na całą szerokość — źródło i tryb obrazu w nim, zasilanie na końcu.
        // Wake-on-LAN na TV jest wyłączony — zasilanie tylko pokazuje stan (ADR 0008).
        Column(Modifier.fillMaxSize()) {
        Header(
            R.drawable.px_tv, "LG OLED", repo.prefs.value.roomName, power = tv.power, onPower = {},
            extra = { selectors(Modifier.width(420.u)) },
        )
        Row(Modifier.fillMaxSize().padding(end = 16.u)) {
            Column(Modifier.weight(0.46f).fillMaxHeight().padding(top = 8.u)) {
                Column(Modifier.weight(1f).fillMaxWidth().padding(horizontal = 24.u), verticalArrangement = Arrangement.spacedBy(10.u)) {
                    Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                        for ((name, logo) in Shortcuts.take(3)) channelTile(Modifier.weight(1f).fillMaxHeight(), name, logo)
                    }
                    Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                        channelTile(Modifier.weight(1f).fillMaxHeight(), Shortcuts[3].first, Shortcuts[3].second)
                        channelsTile(Modifier.weight(1f).fillMaxHeight())
                        Spacer(Modifier.weight(1f))
                    }
                }
                bottom(Modifier.fillMaxWidth().padding(24.u))
            }
            Row(
                Modifier.weight(0.54f).fillMaxHeight().padding(vertical = 16.u),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                volume(Modifier.width(72.u).height(200.u))
                Box(Modifier.weight(1f).padding(horizontal = 12.u), contentAlignment = Alignment.Center) {
                    DPad(Modifier.fillMaxHeight()) { repo.tvKey(it) }
                }
                Rocker("P", Modifier.width(72.u).height(200.u)) { up -> repo.tvChannelStep(up) }
            }
        }
        }
    } else {
        Column(Modifier.fillMaxSize()) {
            Header(R.drawable.px_tv, "LG OLED", repo.prefs.value.roomName, power = tv.power, onPower = {})
            selectors(Modifier.fillMaxWidth().padding(horizontal = 24.u))
            Spacer(Modifier.height(12.u))
            Row(Modifier.fillMaxWidth().height(64.u).padding(horizontal = 24.u), horizontalArrangement = Arrangement.spacedBy(10.u)) {
                for ((name, logo) in Shortcuts) channelTile(Modifier.weight(1f).fillMaxHeight(), name, logo)
                channelsTile(Modifier.weight(1f).fillMaxHeight())
            }
            Row(
                Modifier.fillMaxWidth().weight(1f).padding(horizontal = 16.u, vertical = 16.u),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                volume(Modifier.width(72.u).height(200.u))
                Box(Modifier.weight(1f).padding(horizontal = 12.u), contentAlignment = Alignment.Center) {
                    DPad(Modifier.fillMaxWidth()) { repo.tvKey(it) }
                }
                Rocker("P", Modifier.width(72.u).height(200.u)) { up -> repo.tvChannelStep(up) }
            }
            bottom(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 12.u))
        }
    }

    when (dialog) {
        "source" -> OptionDialog("Źródło", tv.sources.map { it.id to it.label }, tv.source, { dialog = null }, repo::tvSource)
        "picture" -> OptionDialog("Tryb obrazu", tv.pictureModes.map { it.id to it.label }, tv.pictureMode, { dialog = null }, repo::tvPictureMode)
        "channels" -> OptionDialog(
            "Kanały",
            state.channels.map { it.id to "${it.number}  ${it.name}" },
            null,
            { dialog = null },
            onSelect = { id -> state.channels.firstOrNull { it.id == id }?.let { repo.tvChannel(it.name, it.id) } },
        )
    }
}
