package app.couchpad.pilot.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.haptic
import app.couchpad.pilot.ui.theme.u
import kotlin.math.roundToInt

/**
 * Pionowy suwak 0..100 z krokiem 2 (Center, Subwoofer, Tył). Stuknięcie ustawia wartość,
 * przeciąganie wysyła najwyżej 10 poleceń/s i zawsze wartość końcową.
 */
@Composable
fun VerticalSlider(
    label: String,
    value: Int?,
    modifier: Modifier = Modifier,
    pending: Boolean,
    onChange: (Int) -> Unit,
) {
    var dragValue by remember { mutableStateOf<Int?>(null) }
    val throttle = remember { Throttle() }
    val send by rememberUpdatedState(onChange)
    val shown = dragValue ?: value
    val shape = RoundedCornerShape(18.u)
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        T(label, 12, color = C.faint)
        Spacer(Modifier.height(4.u))
        T(shown?.toString() ?: "—", 17, FontWeight.Bold)
        Spacer(Modifier.height(8.u))
        BoxWithConstraints(Modifier.weight(1f).width(64.u).clip(shape).background(C.surface2).border(1.u, C.line, shape)) {
            val h = constraints.maxHeight.toFloat()
            fun at(y: Float) = ((1f - y / h).coerceIn(0f, 1f) * 100f / 2).roundToInt() * 2
            Box(
                Modifier.fillMaxSize()
                    .pointerInput(h) {
                        detectDragGestures(
                            onDragStart = { dragValue = at(it.y) },
                            onDragEnd = { dragValue?.let(send); dragValue = null },
                            onDragCancel = { dragValue = null },
                        ) { change, _ ->
                            val v = at(change.position.y)
                            if (v != dragValue) {
                                dragValue = v
                                if (throttle.ready()) send(v)
                            }
                        }
                    }
                    .pointerInput(h) { detectTapGestures { haptic(); send(at(it.y)) } },
            ) {
                if (shown != null) {
                    Box(
                        Modifier.align(Alignment.BottomCenter).fillMaxWidth().fillMaxHeight(shown / 100f)
                            .clip(shape)
                            .background(if (pending && dragValue == null) C.accentPending else C.accent),
                    )
                }
            }
        }
    }
}
