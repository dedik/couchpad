package app.couchpad.pilot.ui

import androidx.compose.animation.AnimatedVisibility
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.slideInHorizontally
import androidx.compose.animation.slideOutHorizontally
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectHorizontalDragGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.systemGestureExclusion
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import kotlinx.coroutines.delay
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

/**
 * Tarcza (widgets::Thermostat): łuk 270°, duża liczba z jednostką, podpis nad nią,
 * szczegół pod nią, −/+ w przerwie łuku i skala na końcach. **Jedno polecenie na gest:**
 * po puszczeniu palca albo 0,7 s po ostatnim stuknięciu w −/+.
 */
@Composable
fun Dial(
    value: Double?,
    min: Double,
    max: Double,
    step: Double,
    caption: String,
    detail: String?,
    unit: String,
    format: (Double) -> String,
    modifier: Modifier = Modifier,
    enabled: Boolean = true,
    pending: Boolean = false,
    minLabel: String = format(min) + unit,
    maxLabel: String = format(max) + unit,
    onCommit: (Double) -> Unit,
) {
    var local by remember { mutableStateOf<Double?>(null) }
    var tapCommitAt by remember { mutableStateOf(0L) }
    val shown = local ?: value
    val span = (max - min).coerceAtLeast(step)
    fun snap(v: Double) = (((v - min) / step).roundToInt() * step + min).coerceIn(min, max)

    // −/+ zbierają stuknięcia i wysyłają jedno polecenie 0,7 s po ostatnim.
    LaunchedEffect(tapCommitAt) {
        if (tapCommitAt == 0L) return@LaunchedEffect
        delay(700)
        local?.let(onCommit)
        local = null
        tapCommitAt = 0L
    }
    // Nowy stan z domu kasuje lokalną wartość dopiero, gdy nie trwa gest.
    LaunchedEffect(value) { if (tapCommitAt == 0L) local = null }

    BoxWithConstraints(modifier.aspectRatio(1f), contentAlignment = Alignment.Center) {
        val sizePx = constraints.maxWidth.toFloat()
        val stroke = sizePx * 0.07f
        fun valueAt(p: Offset): Double {
            var fromStart = (atan2(p.y - sizePx / 2, p.x - sizePx / 2) * 180 / PI).toFloat() - 135f
            while (fromStart < 0) fromStart += 360f
            if (fromStart > 270f) return if (fromStart < 315f) max else min
            return snap(min + span * fromStart / 270f)
        }
        Canvas(
            Modifier.fillMaxSize().pointerInput(enabled, min, max, step) {
                if (!enabled) return@pointerInput
                detectDragGestures(
                    onDragStart = { local = valueAt(it) },
                    onDragEnd = { local?.let(onCommit); tapCommitAt = 0L },
                    onDragCancel = { local = null },
                ) { change, _ ->
                    val v = valueAt(change.position)
                    if (v != local) { local = v; haptic() }
                }
            },
        ) {
            val inset = stroke / 2 + sizePx * 0.03f
            val arc = Size(size.width - 2 * inset, size.height - 2 * inset)
            val topLeft = Offset(inset, inset)
            drawArc(C.surface3, 135f, 270f, false, topLeft, arc, style = Stroke(stroke, cap = StrokeCap.Round))
            if (enabled && shown != null) {
                val sweep = (270f * ((shown - min) / span)).toFloat().coerceIn(0f, 270f)
                drawArc(
                    if (pending && local == null) C.accent.copy(alpha = 0.55f) else C.accent,
                    135f, sweep, false, topLeft, arc, style = Stroke(stroke, cap = StrokeCap.Round),
                )
                val angle = (135f + sweep) * PI.toFloat() / 180f
                val r = arc.width / 2
                drawCircle(Color.White, stroke * 0.62f, Offset(size.width / 2 + r * cos(angle), size.height / 2 + r * sin(angle)))
            }
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            T(caption, 12, FontWeight.Medium, C.faint)
            Row(verticalAlignment = Alignment.Top) {
                T(shown?.let(format) ?: "—", 64, FontWeight.Bold, if (enabled) C.text else C.faint)
                T(unit, 22, FontWeight.Bold, if (enabled) C.text else C.faint, Modifier.padding(top = 8.u))
            }
            if (detail != null) T(detail, 13, color = C.dim)
            Spacer(Modifier.height(14.u))
            Row(horizontalArrangement = Arrangement.spacedBy(20.u)) {
                for (sign in listOf(-1, 1)) {
                    RoundButton(if (sign < 0) Icons.Rounded.Remove else Icons.Rounded.Add, size = 56, tint = if (enabled) C.text else C.faint) {
                        if (!enabled) return@RoundButton
                        val base = local ?: value ?: return@RoundButton
                        local = snap(base + sign * step)
                        tapCommitAt = System.currentTimeMillis()
                    }
                }
            }
        }
        // Skala na końcach łuku, jak na płytce.
        Row(Modifier.align(Alignment.BottomCenter).fillMaxWidth().padding(start = maxWidth * 0.1f, end = maxWidth * 0.1f, bottom = maxHeight * 0.08f)) {
            T(minLabel, 11, color = C.faint)
            Spacer(Modifier.weight(1f))
            T(maxLabel, 11, color = C.faint)
        }
    }
}

/**
 * Szuflada przy krawędzi ekranu (poziom, home-screen.md §3): przeciągnięcie od krawędzi
 * (pas 28 px, ruch ≥ 36 px) albo dotknięcie pasa ją wysuwa; dotknięcie przyciemnionego tła
 * albo wybór w środku chowa. Uchwyt 5 × 64 px podpowiada, że szuflada jest.
 */
@Composable
fun BoxScope.EdgeDrawer(
    left: Boolean,
    open: Boolean,
    onOpen: (Boolean) -> Unit,
    width: Int,
    edgeInset: androidx.compose.ui.unit.Dp = androidx.compose.ui.unit.Dp(0f),
    content: @Composable () -> Unit,
) {
    val threshold = with(LocalDensity.current) { 36.u.toPx() }
    // Pas przy krawędzi z uchwytem.
    Box(
        Modifier
            .align(if (left) Alignment.CenterStart else Alignment.CenterEnd)
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
                    onDragEnd = { if ((if (left) total else -total) >= threshold) onOpen(true) },
                ) { _, dx -> total += dx }
            }
            .clickable(remember { MutableInteractionSource() }, null) { haptic(); onOpen(true) },
        contentAlignment = if (left) Alignment.CenterStart else Alignment.CenterEnd,
    ) {
        // Uchwyt odsunięty od przedniej kamery, jeśli stoi przy tej krawędzi.
        Box(
            Modifier.padding(start = if (left) edgeInset else 0.u).padding(horizontal = 4.u).width(5.u).height(64.u)
                .clip(RoundedCornerShape(3.u)).background(C.line),
        )
    }
    AnimatedVisibility(open, enter = fadeIn(), exit = fadeOut()) {
        Box(
            Modifier.fillMaxSize().background(Color.Black.copy(alpha = 0.55f))
                .clickable(remember { MutableInteractionSource() }, null) { onOpen(false) },
        )
    }
    AnimatedVisibility(
        open,
        modifier = Modifier.align(if (left) Alignment.CenterStart else Alignment.CenterEnd),
        enter = slideInHorizontally { if (left) -it else it },
        exit = slideOutHorizontally { if (left) -it else it },
    ) {
        Box(
            Modifier.fillMaxHeight().width(width.u).background(C.bg)
                .pointerInput(left) {
                    var total = 0f
                    detectHorizontalDragGestures(
                        onDragStart = { total = 0f },
                        onDragEnd = { if ((if (left) -total else total) >= threshold) onOpen(false) },
                    ) { _, dx -> total += dx }
                },
        ) { content() }
    }
}

/** Siedem próbek barwy od ciepłej do zimnej (home-screen.md §4.5). */
val KelvinColors = listOf(
    Color(0xFFFFBF75), Color(0xFFFFD291), Color(0xFFFFE2B5), Color(0xFFFFF0DA),
    Color(0xFFF6F6F6), Color(0xFFE3ECFF), Color(0xFFCDDCFF),
)

fun kelvinSteps(min: Int, max: Int): List<Int> =
    if (max <= min) List(7) { min } else List(7) { i -> min + (max - min) * i / 6 }

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
    val selected = if (enabled && current > 0) steps.indices.minByOrNull { kotlin.math.abs(steps[it] - current) } else null
    Column {
        T("BARWA", 11, FontWeight.Medium, C.faint)
        Spacer(Modifier.height(8.u))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.u)) {
            steps.forEachIndexed { i, k ->
                val active = i == selected
                val shape = RoundedCornerShape(12.u)
                Box(
                    Modifier.weight(1f).height(52.u).clip(shape)
                        .background(KelvinColors[i].copy(alpha = if (active) 1f else if (enabled) 0.55f else 0.18f))
                        .then(
                            if (active) Modifier.border(3.u, if (pending) C.accent.copy(alpha = 0.5f) else C.accent, shape)
                            else Modifier
                        )
                        .clickable(remember { MutableInteractionSource() }, null) {
                            if (enabled) { haptic(); onSelect(k) }
                        },
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

/** Kafel opcji (home-screen.md §4.4): podpis wersalikami nad wartością. */
@Composable
fun OptionTile(
    caption: String,
    value: String,
    modifier: Modifier = Modifier,
    active: Boolean = false,
    pending: Boolean = false,
    icon: (@Composable () -> Unit)? = null,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(14.u)
    Box(
        modifier.clip(shape)
            .background(if (active) C.accent.copy(alpha = 0.16f) else C.surface2)
            .then(
                if (active || pending) Modifier.border(1.u, C.accent.copy(alpha = if (pending && !active) 0.5f else 1f), shape)
                else Modifier
            )
            .clickable(remember { MutableInteractionSource() }, null) { haptic(); onClick() }
            .padding(horizontal = 14.u, vertical = 10.u),
        contentAlignment = Alignment.CenterStart,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            if (icon != null) { icon(); Spacer(Modifier.width(12.u)) }
            Column(Modifier.weight(1f)) {
                T(caption, 10, FontWeight.Medium, if (active) C.accent else C.faint)
                Spacer(Modifier.height(4.u))
                T(value, 15, FontWeight.Medium, if (active) C.accent else C.text)
            }
        }
    }
}

fun upperPl(s: String) = s.uppercase(java.util.Locale("pl"))
