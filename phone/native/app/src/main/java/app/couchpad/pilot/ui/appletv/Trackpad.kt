package app.couchpad.pilot.ui.appletv

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Home
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
import androidx.compose.ui.draw.drawBehind
import androidx.compose.ui.draw.shadow
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.graphicsLayer
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.components.direction
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LightPalette
import app.couchpad.pilot.ui.theme.LocalUnits
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.haptic
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.delay
import kotlin.math.abs
import kotlin.math.hypot

/** Strefa środka (select): promień jako ułamek krótszego boku. */
private const val CENTER_RATIO = 0.22f
private const val HOLD_MS = 600L

/** Palec stoi, jeśli przez tyle ms nie przesunął się dalej niż [STILL_DP]. */
private const val PAUSE_MS = 100L
private const val STILL_DP = 1.5f

/**
 * Machnięcie to jeden krok. Kolejne kroki tylko przy dalszym przeciąganiu, jak powtarzanie
 * klawisza: drugi po [REPEAT_DELAY_MS], następne co najmniej co [REPEAT_MS]. Ruch w czasie
 * blokady przepada, więc wyhamowanie palca po machnięciu nie dobija kolejnego kroku.
 */
private const val REPEAT_DELAY_MS = 300L
private const val REPEAT_MS = 120L

/**
 * Gładzik jak pilot Siri: stuknięcie strzałki lub środka to pojedynczy klawisz,
 * machnięcie palcem to jeden krok o [stepUnits] (Klawiatura i gładzik → Czułość), przeciąganie powtarza krok,
 * przytrzymanie środka otwiera menu kontekstowe.
 *
 * Wygląd: szklana płyta z phone/docs/TRACKPAD_SPEC.md — zaokrąglony prostokąt z delikatnym
 * gradientem, strzałki pixel-art przy krawędziach i wklęsły krążek z poświatą akcentu na środku
 * (bez ikony i podpisu).
 * Wypełnia [modifier]; kwadrat daje wywołujący (`aspectRatio`).
 */
@Composable
internal fun Trackpad(modifier: Modifier, stepUnits: Int, onKey: (key: String, action: String) -> Unit) {
    var lit by remember { mutableStateOf<String?>(null) }
    var touching by remember { mutableStateOf(false) }
    val key by rememberUpdatedState(onKey)
    val shape = RoundedCornerShape(28.u)
    val light = C.palette == LightPalette
    BoxWithConstraints(modifier) {
        val w = constraints.maxWidth.toFloat()
        val h = constraints.maxHeight.toFloat()
        val density = LocalDensity.current.density
        val units = LocalUnits.current.dpPerUnit
        val stepPx = stepUnits * units * density
        val stillPx = STILL_DP * density
        val centerPx = minOf(w, h) * CENTER_RATIO
        fun send(k: String, action: String = "tap") {
            haptic()
            key(k, action)
        }
        Box(
            Modifier.fillMaxSize().glassSurface(shape, light)
                .pointerInput(w, h, stepPx) {
                    awaitEachGesture {
                        val down = awaitFirstDown()
                        touching = true
                        val start = down.position
                        val downAt = System.currentTimeMillis()
                        val atCenter = hypot(start.x - w / 2, start.y - h / 2) < centerPx
                        var acc = Offset.Zero
                        // Kotwica ruchu: gdzie i kiedy palec ostatnio przesunął się o więcej niż drgnięcie.
                        var anchor = start
                        var movingAt = down.uptimeMillis
                        var steps = 0
                        var stepAt = 0L
                        var moved = false
                        var held = false
                        while (true) {
                            val change = awaitPointerEvent().changes.firstOrNull() ?: break
                            if (!change.pressed) break
                            // Po zatrzymaniu palca reszta ruchu przepada: inaczej przesunięcie punktu
                            // styku przy odrywaniu palca dobija ją do progu i daje dodatkowy krok.
                            if (change.uptimeMillis - movingAt > PAUSE_MS) acc = Offset.Zero
                            if ((change.position - anchor).getDistance() > stillPx) {
                                anchor = change.position
                                movingAt = change.uptimeMillis
                            }
                            val gap = if (steps <= 1) REPEAT_DELAY_MS else REPEAT_MS
                            if (steps > 0 && change.uptimeMillis - stepAt < gap) {
                                acc = Offset.Zero
                            } else {
                                acc += change.positionChange()
                            }
                            if (abs(acc.x) > stepPx || abs(acc.y) > stepPx) {
                                moved = true
                                steps++
                                stepAt = change.uptimeMillis
                                lit = direction(acc.x, acc.y).also { send(it) }
                                acc = Offset.Zero
                            }
                            if (!moved && !held && atCenter && System.currentTimeMillis() - downAt > HOLD_MS) {
                                held = true
                                send("select", "hold")
                            }
                        }
                        if (!moved && !held) {
                            val dx = start.x - w / 2
                            val dy = start.y - h / 2
                            lit = (if (hypot(dx, dy) < centerPx) "select" else direction(dx, dy)).also { send(it) }
                        }
                        lit = null
                        touching = false
                    }
                },
        ) {
            val idle = C.text.copy(alpha = 0.25f)
            PixelArrow("up", lit, idle, Modifier.align(Alignment.TopCenter).padding(top = 14.u))
            PixelArrow("down", lit, idle, Modifier.align(Alignment.BottomCenter).padding(bottom = 32.u))
            PixelArrow("left", lit, idle, Modifier.align(Alignment.CenterStart).padding(start = 16.u))
            PixelArrow("right", lit, idle, Modifier.align(Alignment.CenterEnd).padding(end = 16.u))
            CenterBadge(Modifier.align(Alignment.Center), pressed = touching, light = light)
        }
    }
}

/**
 * Szklana płyta: baza karty, pionowy połysk, ledwie widoczna poświata akcentu na środku,
 * jasna krawędź u góry i cień wewnątrz u dołu. W jasnym stylu piaskowa, jak w specyfikacji.
 * Styl OLED zostaje czarny z samym obrysem — połysk zapaliłby piksele całej płyty.
 */
@Composable
private fun Modifier.glassSurface(shape: Shape, light: Boolean): Modifier {
    val accent = C.accent
    val oled = C.outlined
    val base = C.surface
    val edge = if (oled) C.line else C.text.copy(alpha = 0.08f)
    return this
        .then(if (light) Modifier.shadow(6.u, shape, ambientColor = Color.Black.copy(alpha = 0.06f), spotColor = Color.Black.copy(alpha = 0.06f)) else Modifier)
        .clip(shape)
        .drawBehind {
            if (light) {
                drawRect(Brush.verticalGradient(listOf(Color(0xFFF0ECE4), Color(0xFFE7E2DA))))
            } else {
                drawRect(base)
                if (!oled) drawRect(Brush.verticalGradient(listOf(Color.White.copy(alpha = 0.05f), Color.White.copy(alpha = 0.01f))))
            }
            val glow = if (light) 0.06f else if (oled) 0.03f else 0.04f
            drawRect(
                Brush.radialGradient(
                    0f to accent.copy(alpha = glow), 0.6f to accent.copy(alpha = glow / 3), 1f to Color.Transparent,
                    center = center, radius = size.maxDimension / 2,
                ),
            )
            if (!oled) {
                // Jasna krawędź u góry i ciemny cień wewnątrz u dołu (inset w specyfikacji).
                val rim = (if (light) 0.8f else 0.12f)
                drawRect(Brush.verticalGradient(0f to Color.White.copy(alpha = rim), 0.03f to Color.Transparent))
                drawRect(
                    Brush.verticalGradient(
                        0.94f to Color.Transparent, 1f to Color.Black.copy(alpha = if (light) 0.05f else 0.3f),
                    ),
                )
            }
        }
        .border(1.u, edge, shape)
}

/** Strzałka pixel-art: trójkąt 7 × 4 piksele, obrócony w stronę kierunku. */
@Composable
private fun PixelArrow(dir: String, lit: String?, idle: Color, modifier: Modifier) {
    val color = if (lit == dir) C.accent else idle
    val vertical = dir == "up" || dir == "down"
    Canvas(modifier.size(if (vertical) 11.u else 7.u, if (vertical) 7.u else 11.u)) {
        // Wiersze trójkąta skierowanego w górę: (od, do) w pikselach siatki 7 × 4.
        val rows = listOf(3 to 4, 2 to 5, 1 to 6, 0 to 7)
        val cell = (if (vertical) size.width else size.height) / 7f
        rows.forEachIndexed { i, (from, to) ->
            val depth = when (dir) { "up", "left" -> i; else -> 3 - i }
            if (vertical) {
                drawRect(color, Offset(from * cell, depth * size.height / 4f), Size((to - from) * cell, size.height / 4f))
            } else {
                drawRect(color, Offset(depth * size.width / 4f, from * cell), Size(size.width / 4f, (to - from) * cell))
            }
        }
    }
}

/** Wklęsły krążek 46 jednostek pod opuszek kciuka, z obwódką i poświatą akcentu. */
@Composable
private fun CenterBadge(modifier: Modifier, pressed: Boolean, light: Boolean) {
    val accent = C.accent
    val oled = C.outlined
    val scale = if (pressed) 0.92f else 1f
    val glowAlpha = if (pressed) 0.45f else if (light) 0.25f else 0.22f
    val glowRadius = if (pressed) 24.u else if (light) 12.u else 16.u
    Box(
        modifier.size(46.u).graphicsLayer { scaleX = scale; scaleY = scale }
            .drawBehind {
                val r = size.minDimension / 2
                val spread = glowRadius.toPx()
                drawCircle(
                    Brush.radialGradient(
                        0f to accent.copy(alpha = glowAlpha), (r / (r + spread)) to accent.copy(alpha = glowAlpha),
                        1f to Color.Transparent, center = center, radius = r + spread,
                    ),
                    radius = r + spread,
                )
                val fill = when {
                    light -> listOf(Color.White, Color(0xFFEDE8E0))
                    oled -> listOf(Color.Black, Color.Black)
                    else -> listOf(Color(0xFF222328), Color(0xFF17181C))
                }
                drawCircle(
                    Brush.radialGradient(fill, center = Offset(size.width * 0.35f, size.height * 0.35f), radius = size.minDimension),
                    radius = r,
                )
                if (!oled) {
                    drawCircle(
                        Brush.verticalGradient(
                            0f to Color.White.copy(alpha = if (light) 0.9f else 0.15f), 0.12f to Color.Transparent,
                            0.88f to Color.Transparent, 1f to Color.Black.copy(alpha = if (light) 0.08f else 0.6f),
                        ),
                        radius = r,
                    )
                }
            }
            .border(
                if (light) 1.5f.u else 1.u,
                when {
                    pressed -> accent
                    light -> accent
                    else -> accent.copy(alpha = 0.45f)
                },
                CircleShape,
            ),
    )
}

/** Home: stuknięcie, podwójne (przełącznik aplikacji) i przytrzymanie (Centrum sterowania). */
@Composable
internal fun HomeKey(modifier: Modifier, height: Int = 56, pixel: Int? = null, onAction: (String) -> Unit) {
    var lastTap by remember { mutableStateOf(0L) }
    var pendingTap by remember { mutableStateOf(0L) }
    val action by rememberUpdatedState(onAction)
    // Pojedyncze stuknięcie czeka 0,3 s, czy nie przyjdzie drugie.
    LaunchedEffect(pendingTap) {
        if (pendingTap == 0L) return@LaunchedEffect
        delay(DOUBLE_TAP_MS)
        action("tap")
        pendingTap = 0L
    }
    val shape = RoundedCornerShape(16.u)
    Box(
        modifier.height(height.u).clip(shape).background(C.surface).border(1.u, C.line, shape)
            .pointerInput(Unit) {
                awaitEachGesture {
                    awaitFirstDown()
                    haptic()
                    val downAt = System.currentTimeMillis()
                    var held = false
                    while (true) {
                        val event = awaitPointerEvent()
                        if (event.changes.none { it.pressed }) break
                        if (!held && System.currentTimeMillis() - downAt > 400) {
                            held = true
                            action("hold")
                        }
                    }
                    if (!held) {
                        val now = System.currentTimeMillis()
                        if (pendingTap != 0L && now - lastTap < DOUBLE_TAP_MS) {
                            pendingTap = 0L
                            action("double")
                        } else {
                            pendingTap = now
                        }
                        lastTap = now
                    }
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        if (pixel != null) {
            // Zakładka Pilot: ikona pixel-art nad podpisem, jak pozostałe klawisze.
            Column(horizontalAlignment = Alignment.CenterHorizontally) {
                Pixel(pixel, 24.u)
                Spacer(Modifier.size(4.u))
                T("Home", 12, FontWeight.Medium, C.dim)
            }
        } else {
            Row(verticalAlignment = Alignment.CenterVertically) {
                Icon(Icons.Rounded.Home, null, tint = C.text, modifier = Modifier.size(20.u))
                Spacer(Modifier.size(8.u))
                T("Home", 15, FontWeight.Medium)
            }
        }
    }
}

private const val DOUBLE_TAP_MS = 300L
