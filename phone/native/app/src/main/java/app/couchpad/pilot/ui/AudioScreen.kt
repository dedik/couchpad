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
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.MusicNote
import androidx.compose.material.icons.rounded.Usb
import androidx.compose.material.icons.rounded.Remove
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
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState

/** Wejścia i efekty Z906 — stałe dla modelu, co do znaku jak w tematach kontrolera. */
private val Z906Sources = listOf("PC - analog 5.1", "RCA - stereo", "Optical 1", "Optical 2", "Coaxial", "AUX")
private val Z906Effects = listOf("Stereo 3D", "Stereo 2.1", "Stereo 4.1", "No effect")
private val EffectLabels = mapOf(
    "Stereo 3D" to "Stereo 3D", "Stereo 2.1" to "Stereo 2.1", "Stereo 4.1" to "Stereo 4.1", "No effect" to "Bez efektu",
)

/** 44 poziomy wzmacniacza (0–43): −/+ liczą w poziomach, nie w procentach. */
private const val MaxLevel = 43
private fun levelOf(percent: Int) = (percent * MaxLevel + 50) / 100
private fun percentOf(level: Int) = (level * 100 + MaxLevel / 2) / MaxLevel
internal fun stepVolume(percent: Int, levels: Int, limit: Int): Int {
    val level = (levelOf(percent) + levels).coerceIn(0, MaxLevel)
    var next = percentOf(level)
    if (next > limit) next = percentOf(limit * MaxLevel / 100)
    if (levels > 0 && next < percent) return percent
    if (levels < 0 && next > percent) return percent
    return next
}

@Composable
fun AudioScreen(state: PilotState, repo: PilotRepository) {
    val z = state.z906
    val limit = repo.prefs.value.volumeLimit
    var dialog by remember { mutableStateOf<String?>(null) }
    val pending = state.pending

    val header: @Composable (@Composable () -> Unit) -> Unit = { before ->
        Header(
            R.drawable.px_audio, "Z906", repo.prefs.value.roomName,
            power = z.power, powerPending = "z906.power" in pending,
            extra = { before(); Pill("Decode", z.decode) },
            onPower = { repo.z906Power(it) },
        )
    }
    val selectors: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(12.u)) {
            SelectorButton(z.source, Modifier.weight(1f), "z906.source" in pending, Icons.Rounded.Usb) { dialog = "source" }
            SelectorButton(EffectLabels[z.effect] ?: z.effect, Modifier.weight(1f), "z906.effect" in pending, Icons.Rounded.MusicNote) { dialog = "effect" }
        }
    }
    val dial: @Composable (Modifier) -> Unit = { modifier ->
        VolumeDial(z.volume, limit, modifier, "z906.volume" in pending, onChange = repo::z906Volume) { shown ->
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                RoundButton(
                    if (z.muted) Icons.Rounded.VolumeOff else Icons.Rounded.VolumeUp,
                    size = 46,
                    tint = if (z.muted) C.danger else C.text,
                    border = if (z.muted) C.danger else C.line,
                ) { repo.z906Mute(!z.muted) }
                Spacer(Modifier.height(4.u))
                T(shown?.toString() ?: "—", 64, FontWeight.Bold)
                T("głośność", 12, color = C.faint)
                Spacer(Modifier.height(10.u))
                Row(horizontalArrangement = Arrangement.spacedBy(16.u)) {
                    RoundButton(Icons.Rounded.Remove, size = 52) {
                        z.volume?.let { repo.z906Volume(stepVolume(it, -1, limit)) }
                    }
                    RoundButton(Icons.Rounded.Add, size = 52) {
                        z.volume?.let { repo.z906Volume(stepVolume(it, 1, limit)) }
                    }
                }
            }
        }
    }
    val sliders: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.SpaceBetween) {
            VerticalSlider("Center", z.center, Modifier.width(80.u), "z906.center" in pending) { repo.z906Level("center", it) }
            VerticalSlider("Subwoofer", z.subwoofer, Modifier.width(80.u), "z906.subwoofer" in pending) { repo.z906Level("subwoofer", it) }
            VerticalSlider("Tył", z.rear, Modifier.width(80.u), "z906.rear" in pending) { repo.z906Level("rear", it) }
        }
    }

    if (LocalLandscape.current) {
        Column(Modifier.fillMaxSize()) {
            // Nagłówek na całą szerokość: źródło, efekt i Decode w nim, zasilanie na końcu.
            header { selectors(Modifier.width(420.u)); Spacer(Modifier.width(12.u)) }
            Row(Modifier.fillMaxSize().padding(horizontal = 24.u, vertical = 12.u)) {
                Box(Modifier.weight(0.5f).fillMaxHeight(), contentAlignment = Alignment.Center) { dial(Modifier.fillMaxHeight()) }
                sliders(Modifier.weight(0.5f).fillMaxHeight().padding(start = 24.u))
            }
        }
    } else {
        Column(Modifier.fillMaxSize()) {
            header {}
            selectors(Modifier.fillMaxWidth().padding(horizontal = 24.u))
            Spacer(Modifier.height(8.u))
            Box(Modifier.fillMaxWidth().weight(1.25f).padding(horizontal = 40.u), contentAlignment = Alignment.Center) {
                dial(Modifier.fillMaxWidth())
            }
            sliders(Modifier.fillMaxWidth().weight(1f).padding(horizontal = 40.u, vertical = 12.u))
        }
    }

    when (dialog) {
        "source" -> OptionDialog("Źródło", Z906Sources.map { it to it }, z.source, { dialog = null }, repo::z906Source)
        "effect" -> OptionDialog("Efekt", Z906Effects.map { it to (EffectLabels[it] ?: it) }, z.effect, { dialog = null }, repo::z906Effect)
    }
}
