package app.couchpad.pilot.ui.audio

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectVerticalDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.offset
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.data.Z906
import app.couchpad.pilot.ui.components.RoundButton
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u
import kotlin.math.roundToInt

/**
 * Kanał Z906 (tv-and-z906-ui.md): mała tarcza 270° z wartością w środku, podpis i −/+.
 * Wartość to procent, ale wzmacniacz ma 44 poziomy (0–43), więc −/+ i przeciąganie idą
 * o poziom. Gest jest względny: pionowy ruch po kaflu (≈150 u na pełny zakres), dotknięcie
 * nie skacze do palca. Polecenie po puszczeniu palca, jak na płytce.
 */
@Composable
internal fun ChannelRing(
    label: String,
    value: Int?,
    pending: Boolean,
    modifier: Modifier = Modifier,
    onChange: (Int) -> Unit,
    onStep: (Int) -> Unit,
) {
    var dragLevel by remember { mutableStateOf<Float?>(null) }
    val send by rememberUpdatedState(onChange)
    val current by rememberUpdatedState(value)
    val travel = 150.u
    val shown = dragLevel?.let { Z906.percentOf(it.roundToInt()) } ?: value
    val shape = RoundedCornerShape(14.u)
    val ink = when {
        dragLevel != null || pending -> C.accent
        else -> C.text
    }

    Column(
        modifier
            .clip(shape)
            .background(C.surface2)
            .then(if (pending) Modifier.border(1.u, C.accentPending, shape) else Modifier)
            .pointerInput(Unit) {
                val px = travel.toPx()
                detectVerticalDragGestures(
                    onDragStart = { dragLevel = current?.let { Z906.levelOf(it).toFloat() } },
                    onDragEnd = {
                        val level = dragLevel
                        val v = current
                        if (level != null && v != null) {
                            val next = Z906.percentOf(level.roundToInt())
                            if (next != v) send(next)
                        }
                        dragLevel = null
                    },
                    onDragCancel = { dragLevel = null },
                ) { _, dy ->
                    dragLevel = dragLevel?.let { (it - dy / px * Z906.maxLevel).coerceIn(0f, Z906.maxLevel.toFloat()) }
                }
            }
            .padding(vertical = 12.u),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        Box(Modifier.size(88.u), contentAlignment = Alignment.Center) {
            val track = C.surface3
            val accent = C.accent
            val stroke = 8.u
            Canvas(Modifier.size(88.u)) {
                val w = stroke.toPx()
                val inset = w / 2
                val arcSize = Size(size.width - w, size.height - w)
                val style = Stroke(w, cap = StrokeCap.Round)
                drawArc(track, 135f, 270f, false, Offset(inset, inset), arcSize, style = style)
                if (shown != null && shown > 0) {
                    drawArc(accent, 135f, 270f * shown / 100f, false, Offset(inset, inset), arcSize, style = style)
                }
            }
            T(shown?.toString() ?: "—", 26, FontWeight.Bold, ink)
        }
        // Łuk jest otwarty u dołu: podpis wchodzi w tę przerwę.
        T(label, 11, FontWeight.Medium, C.dim, modifier = Modifier.offset(y = (-10).u))
        Row(horizontalArrangement = Arrangement.spacedBy(10.u), modifier = Modifier.offset(y = (-4).u)) {
            RoundButton(Icons.Rounded.Remove, size = 36, background = C.surface3, border = C.surface3) { onStep(-1) }
            RoundButton(Icons.Rounded.Add, size = 36, background = C.surface3, border = C.surface3) { onStep(1) }
        }
    }
}
