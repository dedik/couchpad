package app.couchpad.pilot.ui.components

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxScope
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.haptic
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.delay
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

/** Geometria łuku 270° z przerwą na dole: start w lewym dole (135°), koniec w prawym. */
internal object ArcGeometry {
    const val START = 135f
    const val SWEEP = 270f

    /**
     * Położenie punktu na łuku jako ułamek 0..1. Punkt w przerwie na dole przyciąga się
     * do bliższego końca.
     */
    fun fractionAt(p: Offset, size: Float): Float {
        var fromStart = (atan2(p.y - size / 2, p.x - size / 2) * 180 / PI).toFloat() - START
        while (fromStart < 0) fromStart += 360f
        if (fromStart > SWEEP) return if (fromStart < SWEEP + (360f - SWEEP) / 2) 1f else 0f
        return fromStart / SWEEP
    }

    /** Wartość zaokrąglona do kroku [step] liczonego od [min], w granicach [min]..[max]. */
    fun snap(value: Double, min: Double, max: Double, step: Double): Double =
        (((value - min) / step).roundToInt() * step + min).coerceIn(min, max)
}

/**
 * Tor łuku z wypełnieniem do [fraction] i uchwytem na końcu; przeciąganie zgłasza ułamek.
 * [fraction] null rysuje sam tor (np. urządzenie wyłączone). Treść stoi na środku.
 *
 * Bez BoxWithConstraints: treść przekazana lambdą do subkompozycji nie odświeżała się po
 * zmianie stanu (np. podpis „WYŁĄCZONA” po włączeniu lampy). Rozmiar dają Canvas i gest.
 */
@Composable
internal fun ArcTrack(
    fraction: Float?,
    color: Color,
    strokeRatio: Float,
    modifier: Modifier = Modifier,
    draggable: Boolean = true,
    onDragStart: (Float) -> Unit = {},
    onDrag: (Float) -> Unit = {},
    onDragEnd: () -> Unit = {},
    onDragCancel: () -> Unit = {},
    content: @Composable BoxScope.() -> Unit,
) {
    // Gest żyje dłużej niż jedna kompozycja — zawsze woła najnowsze akcje.
    val start by rememberUpdatedState(onDragStart)
    val move by rememberUpdatedState(onDrag)
    val end by rememberUpdatedState(onDragEnd)
    val cancel by rememberUpdatedState(onDragCancel)
    Box(modifier.aspectRatio(1f), contentAlignment = Alignment.Center) {
        Canvas(
            Modifier.fillMaxSize().pointerInput(draggable) {
                if (!draggable) return@pointerInput
                val side = size.width.toFloat()
                detectDragGestures(
                    onDragStart = { start(ArcGeometry.fractionAt(it, side)) },
                    onDragEnd = { end() },
                    onDragCancel = { cancel() },
                ) { change, _ -> move(ArcGeometry.fractionAt(change.position, side)) }
            },
        ) {
            val side = size.width
            val stroke = side * strokeRatio
            val inset = stroke / 2 + side * 0.03f
            val arc = Size(size.width - 2 * inset, size.height - 2 * inset)
            val topLeft = Offset(inset, inset)
            val cap = Stroke(stroke, cap = StrokeCap.Round)
            drawArc(C.surface3, ArcGeometry.START, ArcGeometry.SWEEP, false, topLeft, arc, style = cap)
            if (fraction != null) {
                val sweep = ArcGeometry.SWEEP * fraction.coerceIn(0f, 1f)
                drawArc(color, ArcGeometry.START, sweep, false, topLeft, arc, style = cap)
                val angle = (ArcGeometry.START + sweep) * PI.toFloat() / 180f
                val r = arc.width / 2
                drawCircle(Color.White, stroke * 0.62f, Offset(size.width / 2 + r * cos(angle), size.height / 2 + r * sin(angle)))
            }
        }
        content()
    }
}

/**
 * Tarcza (widgets::Thermostat): łuk 270°, duża liczba z jednostką, podpis nad nią,
 * szczegół pod nią, −/+ i skala na końcach łuku. **Jedno polecenie na gest:** po puszczeniu
 * palca albo 0,7 s po ostatnim stuknięciu w −/+ (jak `tile_commit` na płytce).
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
    var dragging by remember { mutableStateOf(false) }
    val commit by rememberUpdatedState(onCommit)
    val shown = local ?: value
    val span = (max - min).coerceAtLeast(step)
    fun snap(v: Double) = ArcGeometry.snap(v, min, max, step)
    fun at(fraction: Float) = snap(min + span * fraction)

    // −/+ zbierają stuknięcia i wysyłają jedno polecenie 0,7 s po ostatnim.
    LaunchedEffect(tapCommitAt) {
        if (tapCommitAt == 0L) return@LaunchedEffect
        delay(700)
        local?.let(commit)
        local = null
        tapCommitAt = 0L
    }
    // Nowy stan z domu kasuje lokalną wartość dopiero, gdy nie trwa gest.
    LaunchedEffect(value) { if (tapCommitAt == 0L && !dragging) local = null }

    val faded = if (enabled) C.text else C.faint
    ArcTrack(
        fraction = if (enabled && shown != null) ((shown - min) / span).toFloat() else null,
        color = if (pending && local == null) C.accentPending else C.accent,
        strokeRatio = 0.07f,
        modifier = modifier,
        draggable = enabled,
        onDragStart = { dragging = true; local = at(it) },
        onDrag = { f -> at(f).let { if (it != local) { local = it; haptic() } } },
        // Po puszczeniu tarcza wraca do stanu z domu: pokazuje „oczekuje”, aż przyjdzie
        // potwierdzenie, a nie lokalną wartość, której dom mógł nie przyjąć.
        onDragEnd = { local?.let(commit); local = null; dragging = false; tapCommitAt = 0L },
        onDragCancel = { local = null; dragging = false },
    ) {
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            T(caption, 12, FontWeight.Medium, C.faint)
            BigValue(shown?.let(format) ?: "—", unit, faded)
            if (detail != null) T(detail, 13, color = C.dim)
            Spacer(Modifier.height(14.u))
            Row(horizontalArrangement = Arrangement.spacedBy(20.u)) {
                for (sign in listOf(-1, 1)) {
                    RoundButton(if (sign < 0) Icons.Rounded.Remove else Icons.Rounded.Add, size = 56, tint = faded) {
                        if (!enabled) return@RoundButton
                        val base = local ?: value ?: return@RoundButton
                        local = snap(base + sign * step)
                        tapCommitAt = System.currentTimeMillis()
                    }
                }
            }
        }
        // Skala na końcach łuku, jak na płytce: 10% od boków, 8% od dołu.
        Box(Modifier.align(Alignment.TopCenter).fillMaxWidth(0.8f).fillMaxHeight(0.92f)) {
            Row(Modifier.align(Alignment.BottomCenter).fillMaxWidth()) {
                T(minLabel, 11, color = C.faint)
                Spacer(Modifier.weight(1f))
                T(maxLabel, 11, color = C.faint)
            }
        }
    }
}

/** Duża liczba z jednostką w indeksie górnym (19,0 °C, 50 %). */
@Composable
fun BigValue(text: String, unit: String, color: Color = C.text) {
    Row(verticalAlignment = Alignment.Top) {
        T(text, 64, FontWeight.Bold, color)
        if (unit.isNotEmpty()) T(unit, 22, FontWeight.Bold, color, Modifier.padding(top = 8.u))
    }
}

/**
 * Tarcza głośności Soundstage: łuk 270° od 0 do 100 % (krok 2), nie wyżej niż [limit].
 * Podczas ruchu wysyła najwyżej 10 poleceń/s i zawsze wartość końcową po puszczeniu.
 */
@Composable
fun VolumeDial(
    value: Int?,
    limit: Int,
    modifier: Modifier = Modifier,
    pending: Boolean,
    onChange: (Int) -> Unit,
    center: @Composable (shown: Int?) -> Unit,
) {
    var dragValue by remember { mutableStateOf<Int?>(null) }
    val throttle = remember { Throttle() }
    val send by rememberUpdatedState(onChange)
    val maxValue by rememberUpdatedState(limit)
    fun at(fraction: Float) = (((fraction * 100f) / 2).roundToInt() * 2).coerceIn(0, 100).coerceAtMost(maxValue)
    val shown = dragValue ?: value
    ArcTrack(
        fraction = shown?.let { it / 100f },
        color = if (pending && dragValue == null) C.accentPending else C.accent,
        strokeRatio = 0.075f,
        modifier = modifier,
        onDragStart = { dragValue = at(it) },
        onDrag = { f ->
            val v = at(f)
            if (v != dragValue) {
                dragValue = v
                if (throttle.ready()) send(v)
            }
        },
        onDragEnd = { dragValue?.let(send); dragValue = null },
        onDragCancel = { dragValue = null },
    ) { center(shown) }
}
