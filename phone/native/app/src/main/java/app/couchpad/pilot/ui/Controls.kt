package app.couchpad.pilot.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
import androidx.compose.foundation.gestures.detectDragGestures
import androidx.compose.foundation.gestures.detectTapGestures
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.heightIn
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.Check
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
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.window.Dialog
import kotlinx.coroutines.delay
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.hypot
import kotlin.math.roundToInt
import kotlin.math.sin

/**
 * Lista wyboru (home-screen.md §4.6): w pionie pełny ekran z nagłówkiem, w poziomie panel
 * 380 px przy prawej krawędzi nad przyciemnionym ekranem. Każda opcja to karta z ikoną,
 * bieżąca ma obwódkę w akcencie i podpis „aktywny”. **Wybór zamyka listę**; Wstecz albo
 * dotknięcie tła zamyka bez zmian.
 */
@Composable
fun OptionDialog(
    title: String,
    options: List<Pair<String, String>>,   // id → etykieta
    selected: String?,
    onDismiss: () -> Unit,
    onSelect: (String) -> Unit,
    icon: ((String) -> androidx.compose.ui.graphics.vector.ImageVector)? = null,
) {
    Dialog(
        onDismissRequest = onDismiss,
        properties = androidx.compose.ui.window.DialogProperties(
            usePlatformDefaultWidth = false,
            decorFitsSystemWindows = false,
        ),
    ) {
        ProvideUnits {
            val landscape = LocalLandscape.current
            val list: @Composable (Modifier) -> Unit = { modifier ->
                Column(modifier.background(C.bg)) {
                    Row(
                        Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 12.u),
                        verticalAlignment = Alignment.CenterVertically,
                    ) {
                        RoundButton(Icons.Rounded.ArrowBackIosNew, size = 44, onClick = onDismiss)
                        Spacer(Modifier.width(12.u))
                        T(title, 20, FontWeight.Bold)
                    }
                    LazyColumn(
                        Modifier.fillMaxWidth().weight(1f),
                        contentPadding = androidx.compose.foundation.layout.PaddingValues(start = 24.u, end = 24.u, bottom = 24.u),
                        verticalArrangement = Arrangement.spacedBy(12.u),
                    ) {
                        items(options) { (id, label) ->
                            val active = id == selected
                            val shape = RoundedCornerShape(16.u)
                            Row(
                                Modifier.fillMaxWidth().height(76.u).clip(shape).background(C.surface)
                                    .border(1.u, if (active) C.accent else C.line, shape)
                                    .clickable(remember { MutableInteractionSource() }, null) {
                                        haptic(); onSelect(id); onDismiss()
                                    }
                                    .padding(horizontal = 20.u),
                                verticalAlignment = Alignment.CenterVertically,
                            ) {
                                if (icon != null) {
                                    Icon(icon(id), null, tint = if (active) C.accent else C.dim, modifier = Modifier.size(24.u))
                                    Spacer(Modifier.width(16.u))
                                }
                                T(label, 15, FontWeight.Medium, C.text, Modifier.weight(1f))
                                if (active) T("aktywny", 12, color = C.faint)
                            }
                        }
                    }
                }
            }
            if (landscape) {
                Row(Modifier.fillMaxSize()) {
                    Box(
                        Modifier.weight(1f).fillMaxHeight().background(Color.Black.copy(alpha = 0.6f))
                            .clickable(remember { MutableInteractionSource() }, null) { onDismiss() },
                    )
                    list(Modifier.width(380.u).fillMaxHeight().systemBarsPadding())
                }
            } else {
                list(Modifier.fillMaxSize().systemBarsPadding())
            }
        }
    }
}

/**
 * Przycisk wyboru (widgets::make_value_segment): mała ikona z lewej, obok bieżąca opcja
 * jako przycisk otwierający listę. Ikony jak na płytce: źródło — błyskawica, obraz — obrazek,
 * wejście Z906 — USB, efekt — nuta.
 */
@Composable
fun SelectorButton(
    value: String,
    modifier: Modifier = Modifier,
    pending: Boolean = false,
    icon: androidx.compose.ui.graphics.vector.ImageVector? = null,
    onClick: () -> Unit,
) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        if (icon != null) {
            Icon(icon, null, tint = C.faint, modifier = Modifier.width(24.u).size(20.u))
            Spacer(Modifier.width(10.u))
        }
        Button(value.ifEmpty { "—" }, Modifier.weight(1f), selected = value.isNotEmpty(), pending = pending, height = 52, textSize = 14, onClick = onClick)
    }
}

/**
 * Krzyżak: cztery strzałki na pierścieniu i środkowy OK. Stuknięcie wybiera kierunek
 * według kąta od środka, jak na płytce.
 */
@Composable
fun DPad(
    modifier: Modifier = Modifier,
    centerLabel: String = "OK",
    accentCenter: Boolean = true,
    onKey: (String) -> Unit,
) {
    var pressed by remember { mutableStateOf<String?>(null) }
    BoxWithConstraints(modifier.aspectRatio(1f)) {
        val sizePx = constraints.maxWidth.toFloat()
        Box(
            Modifier.fillMaxSize().clip(CircleShape).background(C.surface).border(1.u, C.line, CircleShape)
                .pointerInput(Unit) {
                    detectTapGestures(
                        onPress = { offset ->
                            val key = keyAt(offset, sizePx)
                            pressed = key
                            haptic()
                            tryAwaitRelease()
                            pressed = null
                            onKey(key)
                        },
                    )
                },
            contentAlignment = Alignment.Center,
        ) {
            val arrow = 30.u
            val tint = { k: String -> if (pressed == k) C.accent else C.dim }
            Icon(Icons.Rounded.KeyboardArrowUp, null, tint = tint("up"), modifier = Modifier.align(Alignment.TopCenter).padding(top = 18.u).size(arrow))
            Icon(Icons.Rounded.KeyboardArrowDown, null, tint = tint("down"), modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 18.u).size(arrow))
            Icon(Icons.Rounded.KeyboardArrowLeft, null, tint = tint("left"), modifier = Modifier.align(Alignment.CenterStart).padding(start = 18.u).size(arrow))
            Icon(Icons.Rounded.KeyboardArrowRight, null, tint = tint("right"), modifier = Modifier.align(Alignment.CenterEnd).padding(end = 18.u).size(arrow))
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

private fun keyAt(offset: Offset, size: Float): String {
    val dx = offset.x - size / 2
    val dy = offset.y - size / 2
    if (hypot(dx, dy) < size * 0.21f) return "ok"
    return if (kotlin.math.abs(dx) > kotlin.math.abs(dy)) {
        if (dx > 0) "right" else "left"
    } else {
        if (dy > 0) "down" else "up"
    }
}

/** Pionowa para −/+ z podpisem (VOL, P). Przytrzymanie powtarza krok. */
@Composable
fun Rocker(label: String, modifier: Modifier = Modifier, onStep: (Boolean) -> Unit) {
    Column(
        modifier.clip(RoundedCornerShape(40.u)).background(C.surface).border(1.u, C.line, RoundedCornerShape(40.u)),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        RepeatKey("+", Modifier.weight(1f).fillMaxWidth()) { onStep(true) }
        T(label, 12, FontWeight.Medium, C.faint)
        RepeatKey("−", Modifier.weight(1f).fillMaxWidth()) { onStep(false) }
    }
}

@Composable
fun RepeatKey(label: String, modifier: Modifier = Modifier, onStep: () -> Unit) {
    var held by remember { mutableStateOf(false) }
    LaunchedEffect(held) {
        if (held) {
            delay(450)
            while (held) {
                onStep()
                delay(160)
            }
        }
    }
    Box(
        modifier.pointerInput(Unit) {
            detectTapGestures(onPress = {
                haptic()
                onStep()
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

/**
 * Tarcza głośności Soundstage: łuk 270° z przerwą na dole, uchwyt do przeciągania.
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
    var lastSent by remember { mutableStateOf(0L) }
    val shown = dragValue ?: value
    BoxWithConstraints(modifier.aspectRatio(1f), contentAlignment = Alignment.Center) {
        val sizePx = constraints.maxWidth.toFloat()
        val stroke = sizePx * 0.075f
        Canvas(
            Modifier.fillMaxSize().pointerInput(limit) {
                detectDragGestures(
                    onDragStart = { offset ->
                        val v = valueAt(offset, sizePx) ?: return@detectDragGestures
                        dragValue = v.coerceAtMost(limit)
                    },
                    onDragEnd = {
                        dragValue?.let(onChange)
                        dragValue = null
                    },
                    onDragCancel = { dragValue = null },
                ) { change, _ ->
                    val v = valueAt(change.position, sizePx) ?: return@detectDragGestures
                    val limited = v.coerceAtMost(limit)
                    if (limited != dragValue) {
                        dragValue = limited
                        val now = System.currentTimeMillis()
                        if (now - lastSent >= 100) {
                            lastSent = now
                            onChange(limited)
                        }
                    }
                }
            },
        ) {
            val inset = stroke / 2 + sizePx * 0.03f
            val arcSize = Size(size.width - 2 * inset, size.height - 2 * inset)
            val topLeft = Offset(inset, inset)
            drawArc(C.surface3, 135f, 270f, false, topLeft, arcSize, style = Stroke(stroke, cap = StrokeCap.Round))
            if (shown != null) {
                val sweep = 270f * shown / 100f
                drawArc(
                    if (pending && dragValue == null) C.accent.copy(alpha = 0.55f) else C.accent,
                    135f, sweep, false, topLeft, arcSize, style = Stroke(stroke, cap = StrokeCap.Round),
                )
                val angle = (135f + sweep) * PI.toFloat() / 180f
                val r = arcSize.width / 2
                val c = Offset(size.width / 2 + r * cos(angle), size.height / 2 + r * sin(angle))
                drawCircle(Color.White, stroke * 0.62f, c)
            }
        }
        center(shown)
    }
}

/** Wartość 0..100 (krok 2) dla punktu na tarczy; null w przerwie na dole. */
private fun valueAt(p: Offset, size: Float): Int? {
    val dx = p.x - size / 2
    val dy = p.y - size / 2
    var deg = (atan2(dy, dx) * 180 / PI).toFloat()   // 0 = w prawo, 90 = w dół
    var fromStart = deg - 135f
    while (fromStart < 0) fromStart += 360f
    if (fromStart > 270f) {
        // Przerwa na dole: przyciągamy do bliższego końca.
        return if (fromStart < 315f) 100 else 0
    }
    val v = (fromStart / 270f * 100f)
    return ((v / 2).roundToInt() * 2).coerceIn(0, 100)
}

/** Pionowy suwak 0..100 z krokiem 2 (Center, Subwoofer, Tył). */
@Composable
fun VerticalSlider(
    label: String,
    value: Int?,
    modifier: Modifier = Modifier,
    pending: Boolean,
    onChange: (Int) -> Unit,
) {
    var dragValue by remember { mutableStateOf<Int?>(null) }
    var lastSent by remember { mutableStateOf(0L) }
    val shown = dragValue ?: value
    Column(modifier, horizontalAlignment = Alignment.CenterHorizontally) {
        T(label, 12, color = C.faint)
        Spacer(Modifier.height(4.u))
        T(shown?.toString() ?: "—", 17, FontWeight.Bold)
        Spacer(Modifier.height(8.u))
        BoxWithConstraints(
            Modifier.weight(1f).width(64.u).clip(RoundedCornerShape(18.u)).background(C.surface2)
                .border(1.u, C.line, RoundedCornerShape(18.u)),
        ) {
            val h = constraints.maxHeight.toFloat()
            fun at(y: Float) = (((1f - (y / h)).coerceIn(0f, 1f) * 100f / 2).roundToInt() * 2)
            Box(
                Modifier.fillMaxSize().pointerInput(Unit) {
                    detectDragGestures(
                        onDragStart = { dragValue = at(it.y) },
                        onDragEnd = { dragValue?.let(onChange); dragValue = null },
                        onDragCancel = { dragValue = null },
                    ) { change, _ ->
                        val v = at(change.position.y)
                        if (v != dragValue) {
                            dragValue = v
                            val now = System.currentTimeMillis()
                            if (now - lastSent >= 100) { lastSent = now; onChange(v) }
                        }
                    }
                }.pointerInput(Unit) {
                    detectTapGestures { haptic(); onChange(at(it.y)) }
                },
            ) {
                if (shown != null) {
                    Box(
                        Modifier.align(Alignment.BottomCenter).fillMaxWidth().fillMaxHeight(shown / 100f)
                            .clip(RoundedCornerShape(18.u))
                            .background(if (pending && dragValue == null) C.accent.copy(alpha = 0.55f) else C.accent),
                    )
                }
            }
        }
    }
}

/** Mała pigułka informacyjna (Decode 5.1). */
@Composable
fun Pill(text: String, active: Boolean) {
    Row(
        Modifier.clip(RoundedCornerShape(20.u)).background(if (active) C.accent.copy(alpha = 0.16f) else C.surface2)
            .border(1.u, if (active) C.accent else C.line, RoundedCornerShape(20.u))
            .padding(horizontal = 12.u, vertical = 6.u),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(7.u).clip(CircleShape).background(if (active) C.accent else C.faint))
        Spacer(Modifier.width(6.u))
        T(text, 12, FontWeight.Medium, if (active) C.accent else C.dim)
    }
}

fun Modifier.noRipple(onClick: () -> Unit) = this.then(
    Modifier.clickable(MutableInteractionSource(), null) { haptic(); onClick() }
)
