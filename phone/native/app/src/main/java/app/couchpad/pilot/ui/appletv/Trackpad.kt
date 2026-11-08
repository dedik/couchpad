package app.couchpad.pilot.ui.appletv

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
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
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.components.ArrowRing
import app.couchpad.pilot.ui.components.direction
import app.couchpad.pilot.ui.components.keyAt
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalUnits
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.haptic
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.delay
import kotlin.math.abs
import kotlin.math.hypot

/** Środek gładzika: promień jako ułamek boku. */
private const val CENTER_RATIO = 0.22f
private const val HOLD_MS = 600L

/**
 * Gładzik jak pilot Siri: stuknięcie strzałki lub środka to pojedynczy klawisz,
 * przesunięcie palcem przewija listy krokami [stepUnits] (Klawiatura i gładzik → Czułość),
 * przytrzymanie środka otwiera menu kontekstowe.
 */
@Composable
internal fun Trackpad(modifier: Modifier, stepUnits: Int, onKey: (key: String, action: String) -> Unit) {
    var lit by remember { mutableStateOf<String?>(null) }
    val key by rememberUpdatedState(onKey)
    BoxWithConstraints(modifier.aspectRatio(1f)) {
        val sizePx = constraints.maxWidth.toFloat()
        val stepPx = stepUnits * LocalUnits.current.dpPerUnit * LocalDensity.current.density
        fun send(k: String, action: String = "tap") {
            haptic()
            key(k, action)
        }
        Box(
            Modifier.fillMaxSize().clip(CircleShape).background(C.surface).border(1.u, C.line, CircleShape)
                .pointerInput(sizePx, stepPx) {
                    awaitEachGesture {
                        val start = awaitFirstDown().position
                        val downAt = System.currentTimeMillis()
                        val atCenter = hypot(start.x - sizePx / 2, start.y - sizePx / 2) < sizePx * CENTER_RATIO
                        var acc = Offset.Zero
                        var moved = false
                        var held = false
                        while (true) {
                            val change = awaitPointerEvent().changes.firstOrNull() ?: break
                            if (!change.pressed) break
                            acc += change.positionChange()
                            if (abs(acc.x) > stepPx || abs(acc.y) > stepPx) {
                                moved = true
                                lit = direction(acc.x, acc.y).also { send(it) }
                                acc = Offset.Zero
                            }
                            if (!moved && !held && atCenter && System.currentTimeMillis() - downAt > HOLD_MS) {
                                held = true
                                send("select", "hold")
                            }
                        }
                        if (!moved && !held) lit = keyAt(start, sizePx, CENTER_RATIO, "select").also { send(it) }
                        lit = null
                    }
                },
            contentAlignment = Alignment.Center,
        ) {
            ArrowRing(lit, inset = 20, size = 28, idle = C.faint)
            Box(
                Modifier.fillMaxSize(0.4f).clip(CircleShape).background(if (lit == "select") C.surface3 else C.surface2)
                    .border(1.u, C.line, CircleShape),
                contentAlignment = Alignment.Center,
            ) { T("OK", 14, FontWeight.Medium, C.faint) }
        }
    }
}

/** Home: stuknięcie, podwójne (przełącznik aplikacji) i przytrzymanie (Centrum sterowania). */
@Composable
internal fun HomeKey(modifier: Modifier, onAction: (String) -> Unit) {
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
        modifier.height(56.u).clip(shape).background(C.surface).border(1.u, C.line, shape)
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
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Rounded.Home, null, tint = C.text, modifier = Modifier.size(20.u))
            Spacer(Modifier.size(8.u))
            T("Home", 15, FontWeight.Medium)
        }
    }
}

private const val DOUBLE_TAP_MS = 300L
