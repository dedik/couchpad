package app.couchpad.pilot.ui.components

import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.composed
import app.couchpad.pilot.ui.theme.haptic

/**
 * Stuknięcie jak na płytce: bez fali Materiala, z krótką wibracją ([feedback] = false dla
 * tła, które tylko zamyka). Wyłączona kontrolka nadal zabiera stuknięcie (nie przechodzi
 * ono do karty pod spodem), ale nic nie robi.
 */
fun Modifier.tap(
    enabled: Boolean = true,
    interaction: MutableInteractionSource? = null,
    feedback: Boolean = true,
    onClick: () -> Unit,
): Modifier = composed {
    clickable(interaction ?: remember { MutableInteractionSource() }, indication = null) {
        if (enabled) {
            if (feedback) haptic()
            onClick()
        }
    }
}

/** Podwójne stuknięcie: budzi wygaszacz, ignoruje pojedyncze przypadkowe dotknięcia. */
fun Modifier.doubleTap(
    feedback: Boolean = true,
    timeoutMs: Long = 400L,
    onDoubleTap: () -> Unit,
): Modifier = composed {
    var lastTapTime by remember { mutableLongStateOf(0L) }
    clickable(remember { MutableInteractionSource() }, indication = null) {
        val now = System.currentTimeMillis()
        if (now - lastTapTime <= timeoutMs) {
            lastTapTime = 0L
            if (feedback) haptic()
            onDoubleTap()
        } else {
            lastTapTime = now
        }
    }
}

/** Zasłona: zabiera dotyk, żeby nie trafił w kontrolki pod spodem. */
fun Modifier.blockTouches(): Modifier = tap(enabled = false) {}

/**
 * Najwyżej jedno polecenie na [periodMs] podczas przeciągania (np. 10/s dla głośności).
 * Wartość końcową po puszczeniu palca wysyła się zawsze, poza ogranicznikiem.
 */
internal class Throttle(private val periodMs: Long = 100) {
    private var last = 0L

    fun ready(now: Long = System.currentTimeMillis()): Boolean {
        if (now - last < periodMs) return false
        last = now
        return true
    }
}
