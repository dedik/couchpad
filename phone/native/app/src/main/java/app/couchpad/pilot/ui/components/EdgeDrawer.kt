package app.couchpad.pilot.ui.components

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.background
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.systemGestureExclusion
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.dp
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.u

/**
 * Szuflada przy krawędzi ekranu (poziom, home-screen.md §3): przeciągnięcie od krawędzi
 * (ruch ≥ 36 px) albo dotknięcie pasa ją wysuwa; dotknięcie przyciemnionego tła albo wybór
 * w środku chowa. Uchwyt 5 × 64 px podpowiada, że szuflada jest.
 */
@Composable
fun BoxScope.EdgeDrawer(
    left: Boolean,
    open: Boolean,
    onOpen: (Boolean) -> Unit,
    width: Int,
    edgeInset: Dp = 0.dp,
    content: @Composable () -> Unit,
) {
    val threshold = with(LocalDensity.current) { 36.u.toPx() }
    val setOpen by rememberUpdatedState(onOpen)
    val edge = if (left) Alignment.CenterStart else Alignment.CenterEnd
    /** Przesunięcie dodatnie znaczy „do środka ekranu”. */
    fun inward(dx: Float) = if (left) dx else -dx

    // Pas przy krawędzi z uchwytem.
    Box(
        Modifier
            .align(edge)
            // Android oddaje aplikacji najwyżej 200 dp krawędzi na własne gesty,
            // więc pas szuflady stoi na środku wysokości, przy uchwycie.
            .fillMaxHeight(0.6f)
            // Szerzej niż 28 px z makiety: w poziomie prawa krawędź to dół telefonu, gdzie
            // Samsung trzyma obowiązkowy gest „ekran główny” — ruch zaczęty dalej od krawędzi
            // trafia już do szuflady.
            .width(48.u)
            .systemGestureExclusion()
            .pointerInput(left) {
                var total = 0f
                detectHorizontalDragGestures(
                    onDragStart = { total = 0f },
                    onDragEnd = { if (inward(total) >= threshold) setOpen(true) },
                ) { _, dx -> total += dx }
            }
            .tap { setOpen(true) },
        contentAlignment = edge,
    ) {
        // Uchwyt odsunięty od przedniej kamery, jeśli stoi przy tej krawędzi.
        Box(
            Modifier.padding(start = if (left) edgeInset else 0.dp).padding(horizontal = 4.u).width(5.u).height(64.u)
                .clip(RoundedCornerShape(3.u)).background(C.line),
        )
    }
    AnimatedVisibility(open, enter = fadeIn(), exit = fadeOut()) {
        Box(Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.55f)).tap(feedback = false) { setOpen(false) })
    }
    AnimatedVisibility(
        open,
        modifier = Modifier.align(edge),
        enter = slideInHorizontally { if (left) -it else it },
        exit = slideOutHorizontally { if (left) -it else it },
    ) {
        Box(
            Modifier.fillMaxHeight().width(width.u).background(C.bg)
                .pointerInput(left) {
                    var total = 0f
                    detectHorizontalDragGestures(
                        onDragStart = { total = 0f },
                        onDragEnd = { if (-inward(total) >= threshold) setOpen(false) },
                    ) { _, dx -> total += dx }
                },
        ) { content() }
    }
}
