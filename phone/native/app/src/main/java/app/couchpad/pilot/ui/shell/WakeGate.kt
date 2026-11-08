package app.couchpad.pilot.ui.shell

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.WarningAmber
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import app.couchpad.pilot.data.LinkPhase
import app.couchpad.pilot.data.LinkState
import app.couchpad.pilot.ui.components.Button
import app.couchpad.pilot.ui.components.StatusDot
import app.couchpad.pilot.ui.components.blockTouches
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.style
import app.couchpad.pilot.ui.theme.u

/**
 * Ekran łączenia zakrywa sterowanie, dopóki po wybudzeniu nie przyjdzie świeży stan.
 * Nie pokazujemy zapamiętanego stanu jako aktualnego (ADR 0005).
 */
@Composable
fun WakeGate(link: LinkState, timedOut: Boolean, onRetry: () -> Unit, onSettings: () -> Unit) {
    if (link.fresh) return
    val failed = link.phase == LinkPhase.Error || timedOut
    Column(
        Modifier.fillMaxSize().background(C.bg).blockTouches().padding(horizontal = 48.u),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Box(
            Modifier.size(88.u).clip(CircleShape)
                .background(if (failed) C.danger.copy(alpha = 0.14f) else C.accent.copy(alpha = 0.12f)),
            contentAlignment = Alignment.Center,
        ) {
            if (failed) {
                Icon(Icons.Rounded.WarningAmber, null, tint = C.danger, modifier = Modifier.size(36.u))
            } else {
                CircularProgressIndicator(color = C.accent, strokeWidth = 3.u, modifier = Modifier.size(40.u))
            }
        }
        Spacer(Modifier.height(32.u))
        T(if (failed) "Brak połączenia z domem" else "Łączenie z domem", 26, FontWeight.Bold)
        Spacer(Modifier.height(10.u))
        Text(
            when {
                !failed -> "Pobieram aktualny stan urządzeń"
                link.error != null -> link.error + ". Nie pokazuję zapamiętanego stanu."
                else -> "Dom nie odpowiedział. Nie pokazuję zapamiętanego stanu."
            },
            style = style(14, color = C.dim),
            textAlign = TextAlign.Center,
        )
        Spacer(Modifier.height(20.u))
        Row(horizontalArrangement = Arrangement.spacedBy(16.u)) {
            StatusDot("MQTT", if (link.connected) C.ok else if (failed) C.danger else C.faint)
            StatusDot("Synchronizacja", if (failed && link.connected) C.danger else C.faint)
        }
        if (failed) {
            Spacer(Modifier.height(28.u))
            Button("Ponów", Modifier.width(240.u), icon = Icons.Rounded.Refresh, accent = true, onClick = onRetry)
            Spacer(Modifier.height(12.u))
            Button("Ustawienia", Modifier.width(240.u), icon = Icons.Rounded.Settings, onClick = onSettings)
        }
    }
}
