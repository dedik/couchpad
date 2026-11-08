package app.couchpad.pilot.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.KeyboardArrowDown
import androidx.compose.material.icons.rounded.KeyboardArrowLeft
import androidx.compose.material.icons.rounded.KeyboardArrowRight
import androidx.compose.material.icons.rounded.KeyboardArrowUp
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.haptic
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.delay
import kotlin.math.abs
import kotlin.math.hypot

/** Kierunek strzałki dla przesunięcia od środka: oś o większym ruchu wygrywa. */
fun direction(dx: Float, dy: Float): String =
    if (abs(dx) > abs(dy)) (if (dx > 0) "right" else "left") else (if (dy > 0) "down" else "up")

/**
 * Klawisz pod punktem okrągłego pola o boku [size]: środek w promieniu [centerRatio] boku
 * to [center], reszta to kierunek według kąta, jak na płytce.
 */
fun keyAt(p: Offset, size: Float, centerRatio: Float, center: String): String {
    val dx = p.x - size / 2
    val dy = p.y - size / 2
    return if (hypot(dx, dy) < size * centerRatio) center else direction(dx, dy)
}

/** Cztery strzałki przy krawędziach okrągłego pola; [lit] świeci w akcencie. */
@Composable
fun BoxScope.ArrowRing(lit: String?, inset: Int, size: Int, idle: Color) {
    val arrows = listOf(
        Triple("up", Icons.Rounded.KeyboardArrowUp, Alignment.TopCenter),
        Triple("down", Icons.Rounded.KeyboardArrowDown, Alignment.BottomCenter),
        Triple("left", Icons.Rounded.KeyboardArrowLeft, Alignment.CenterStart),
        Triple("right", Icons.Rounded.KeyboardArrowRight, Alignment.CenterEnd),
    )
    for ((key, icon, alignment) in arrows) {
        Icon(
            icon, null, tint = if (lit == key) C.accent else idle,
            modifier = Modifier.align(alignment).padding(inset.u).size(size.u),
        )
    }
}

/**
 * Krzyżak: cztery strzałki na pierścieniu i środkowy OK. Stuknięcie wybiera kierunek
 * według kąta od środka; klawisz idzie po puszczeniu palca.
 */
@Composable
fun DPad(
    modifier: Modifier = Modifier,
    centerLabel: String = "OK",
    accentCenter: Boolean = true,
    onKey: (String) -> Unit,
) {
    var pressed by remember { mutableStateOf<String?>(null) }
    val key by rememberUpdatedState(onKey)
    BoxWithConstraints(modifier.aspectRatio(1f)) {
        val sizePx = constraints.maxWidth.toFloat()
        Box(
            Modifier.fillMaxSize().clip(CircleShape).background(C.surface).border(1.u, C.line, CircleShape)
                .pointerInput(sizePx) {
                    detectTapGestures(onPress = { offset ->
                        val k = keyAt(offset, sizePx, 0.21f, "ok")
                        pressed = k
                        haptic()
                        tryAwaitRelease()
                        pressed = null
                        key(k)
                    })
                },
            contentAlignment = Alignment.Center,
        ) {
            ArrowRing(pressed, inset = 18, size = 30, idle = C.dim)
            val centerPressed = pressed == "ok"
            Box(
                Modifier.fillMaxSize(0.42f).clip(CircleShape)
                    .background(
                        when {
                            accentCenter && centerPressed -> C.accentPressed
                            accentCenter -> C.accent
                            centerPressed -> C.surface3
                            else -> C.surface2
                        }
                    )
                    .border(1.u, if (accentCenter) Color.Transparent else C.line, CircleShape),
                contentAlignment = Alignment.Center,
            ) {
                T(centerLabel, 16, FontWeight.Bold, if (accentCenter) C.accentInk else C.dim)
            }
        }
    }
}

/** Pionowa para −/+ z podpisem (VOL, P). Przytrzymanie powtarza krok. */
@Composable
fun Rocker(label: String, modifier: Modifier = Modifier, onStep: (up: Boolean) -> Unit) {
    val shape = RoundedCornerShape(40.u)
    Column(
        modifier.clip(shape).background(C.surface).border(1.u, C.line, shape),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        RepeatKey("+", Modifier.weight(1f).fillMaxWidth()) { onStep(true) }
        T(label, 12, FontWeight.Medium, C.faint)
        RepeatKey("−", Modifier.weight(1f).fillMaxWidth()) { onStep(false) }
    }
}

/** Klawisz z powtarzaniem: krok od razu, po 0,45 s co 0,16 s, dopóki palec leży. */
@Composable
fun RepeatKey(label: String, modifier: Modifier = Modifier, onStep: () -> Unit) {
    var held by remember { mutableStateOf(false) }
    val step by rememberUpdatedState(onStep)
    LaunchedEffect(held) {
        if (!held) return@LaunchedEffect
        delay(450)
        while (held) {
            step()
            delay(160)
        }
    }
    Box(
        modifier.pointerInput(Unit) {
            detectTapGestures(onPress = {
                haptic()
                step()
                held = true
                tryAwaitRelease()
                held = false
            })
        },
        contentAlignment = Alignment.Center,
    ) {
        T(label, 30, FontWeight.Medium, if (held) C.accent else C.text)
    }
}
