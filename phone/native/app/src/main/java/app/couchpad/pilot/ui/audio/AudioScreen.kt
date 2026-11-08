package app.couchpad.pilot.ui.audio

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
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.MusicNote
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.material.icons.rounded.Usb
import androidx.compose.material.icons.rounded.VolumeOff
import androidx.compose.material.icons.rounded.VolumeUp
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.data.Z906
import app.couchpad.pilot.data.Z906Channel
import app.couchpad.pilot.data.Z906State
import app.couchpad.pilot.data.level
import app.couchpad.pilot.ui.components.DeviceHeader
import app.couchpad.pilot.ui.components.OptionDialog
import app.couchpad.pilot.ui.components.Pill
import app.couchpad.pilot.ui.components.RoundButton
import app.couchpad.pilot.ui.components.SelectorButton
import app.couchpad.pilot.ui.components.VolumeDial
import app.couchpad.pilot.ui.components.asChoices
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

private enum class AudioDialog { Source, Effect }

/** Z906 w układzie Soundstage (tv-and-z906-ui.md): tarcza głośności i trzy mini-tarcze kanałów. */
@Composable
fun AudioScreen(state: PilotState, prefs: Preferences, repo: PilotRepository) {
    val z = state.z906
    var dialog by remember { mutableStateOf<AudioDialog?>(null) }
    val landscape = LocalLandscape.current

    val selectors: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(12.u)) {
            SelectorButton(z.source, Modifier.weight(1f), state.isPending(Pending.Z906.source), Icons.Rounded.Usb) {
                dialog = AudioDialog.Source
            }
            SelectorButton(Z906.effectLabel(z.effect), Modifier.weight(1f), state.isPending(Pending.Z906.effect), Icons.Rounded.MusicNote) {
                dialog = AudioDialog.Effect
            }
        }
    }
    val dial: @Composable (Modifier) -> Unit = { modifier ->
        VolumeDial(z.volume, prefs.volumeLimit, modifier, state.isPending(Pending.Z906.volume), onChange = repo::z906Volume) { shown ->
            VolumeCenter(z, shown, onMute = { repo.z906Mute(!z.muted) }, onStep = repo::z906VolumeStep)
        }
    }
    // Kanały jako mini-tarcze z −/+ (o poziom wzmacniacza), bez nagłówka sekcji.
    val channels: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(10.u)) {
            for (channel in Z906Channel.values()) {
                ChannelRing(
                    channel.label.uppercase(), z.level(channel), state.isPending(Pending.Z906.level(channel)),
                    Modifier.weight(1f),
                    onChange = { repo.z906Level(channel, it) },
                    onStep = { repo.z906LevelStep(channel, it) },
                )
            }
        }
    }

    Column(Modifier.fillMaxSize()) {
        // Poziom: źródło i efekt w nagłówku, obok Decode i zasilanie.
        DeviceHeader(
            R.drawable.px_audio, "Z906", prefs.roomName,
            power = z.power, powerPending = state.isPending(Pending.Z906.power),
            extra = {
                if (landscape) {
                    selectors(Modifier.width(420.u))
                    Spacer(Modifier.width(12.u))
                }
                Pill("Decode", z.decode)
            },
            onPower = { repo.z906Power(it) },
        )
        if (landscape) {
            Row(Modifier.fillMaxSize().padding(horizontal = 24.u, vertical = 12.u)) {
                Box(Modifier.weight(0.5f).fillMaxHeight(), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxHeight()) }
                Box(Modifier.weight(0.5f).fillMaxHeight().padding(start = 24.u), contentAlignment = Alignment.Center) {
                    channels(Modifier.fillMaxWidth())
                }
            }
        } else {
            selectors(Modifier.fillMaxWidth().padding(horizontal = 24.u))
            Spacer(Modifier.height(8.u))
            Box(Modifier.fillMaxWidth().weight(1.25f).padding(horizontal = 40.u), contentAlignment = Alignment.Center) {
                dial(Modifier.fillMaxWidth())
            }
            channels(Modifier.fillMaxWidth().padding(start = 24.u, end = 24.u, top = 8.u, bottom = 20.u))
        }
    }

    val close = { dialog = null }
    when (dialog) {
        AudioDialog.Source -> OptionDialog("Źródło", Z906.sources.asChoices(), z.source, close, onSelect = repo::z906Source)
        AudioDialog.Effect -> OptionDialog("Efekt", Z906.effects.asChoices(Z906::effectLabel), z.effect, close, onSelect = repo::z906Effect)
        null -> Unit
    }
}

/** Środek tarczy: wyciszenie, głośność i −/+ o jeden poziom wzmacniacza. */
@Composable
private fun VolumeCenter(z: Z906State, shown: Int?, onMute: () -> Unit, onStep: (Int) -> Unit) {
    Column(horizontalAlignment = Alignment.CenterHorizontally) {
        RoundButton(
            if (z.muted) Icons.Rounded.VolumeOff else Icons.Rounded.VolumeUp,
            size = 46,
            tint = if (z.muted) C.danger else C.text,
            border = if (z.muted) C.danger else C.line,
            onClick = onMute,
        )
        Spacer(Modifier.height(4.u))
        T(shown?.toString() ?: "—", 64, FontWeight.Bold)
        T("głośność", 12, color = C.faint)
        Spacer(Modifier.height(10.u))
        Row(horizontalArrangement = Arrangement.spacedBy(16.u)) {
            RoundButton(Icons.Rounded.Remove, size = 52) { onStep(-1) }
            RoundButton(Icons.Rounded.Add, size = 52) { onStep(1) }
        }
    }
}
