package app.couchpad.pilot.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u
import kotlin.math.abs

/** Siedem próbek barwy od ciepłej do zimnej (home-screen.md §4.5). */
private val KelvinColors = listOf(
    Color(0xFFFFBF75), Color(0xFFFFD291), Color(0xFFFFE2B5), Color(0xFFFFF0DA),
    Color(0xFFF6F6F6), Color(0xFFE3ECFF), Color(0xFFCDDCFF),
)

/** Siedem temperatur barwowych równo rozłożonych w zakresie lampy. */
fun kelvinSteps(min: Int, max: Int): List<Int> =
    if (max <= min) List(KelvinColors.size) { min } else List(KelvinColors.size) { i -> min + (max - min) * i / 6 }

/** Próbki barwy: zaznaczona najbliższa bieżącej temperaturze; wyłączone są przygaszone. */
@Composable
fun KelvinSwatches(
    min: Int,
    max: Int,
    current: Int,
    enabled: Boolean,
    note: String?,
    pending: Boolean,
    onSelect: (Int) -> Unit,
) {
    val steps = kelvinSteps(min, max)
    val selected = if (enabled && current > 0) steps.indices.minByOrNull { abs(steps[it] - current) } else null
    Column {
        Caption("BARWA")
        Spacer(Modifier.height(8.u))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.u)) {
            steps.forEachIndexed { i, kelvin ->
                val active = i == selected
                val shape = RoundedCornerShape(12.u)
                Box(
                    Modifier.weight(1f).height(52.u).clip(shape)
                        .background(KelvinColors[i].copy(alpha = if (active) 1f else if (enabled) 0.55f else 0.18f))
                        .then(if (active) Modifier.border(3.u, if (pending) C.accent.copy(alpha = 0.5f) else C.accent, shape) else Modifier)
                        .tap(enabled) { onSelect(kelvin) },
                )
            }
        }
        Spacer(Modifier.height(6.u))
        Row(Modifier.fillMaxWidth()) {
            T("ciepła", 11, color = C.faint)
            Spacer(Modifier.weight(1f))
            if (note != null) T(note, 11, color = C.faint)
            Spacer(Modifier.weight(1f))
            T("zimna", 11, color = C.faint)
        }
    }
}
