package app.couchpad.pilot.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.window.Dialog
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.ProvideUnits
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.style
import app.couchpad.pilot.ui.theme.u

/** Okno na środku ekranu: zaokrąglona karta z tytułem. */
@Composable
fun DialogCard(title: String, onDismiss: () -> Unit, content: @Composable ColumnScope.() -> Unit) {
    Dialog(onDismissRequest = onDismiss) {
        ProvideUnits {
            val shape = RoundedCornerShape(24.u)
            Column(
                Modifier.fillMaxWidth().clip(shape).background(C.surface).border(1.u, C.line, shape).padding(22.u),
                verticalArrangement = Arrangement.spacedBy(14.u),
            ) {
                T(title, 19, FontWeight.Bold)
                content()
            }
        }
    }
}

/** Potwierdzenie nieodwracalnej akcji: Anuluj i akcja w akcencie. */
@Composable
fun ConfirmDialog(title: String, text: String, action: String, onDismiss: () -> Unit, onConfirm: () -> Unit) {
    DialogCard(title, onDismiss) {
        Text(text, style = style(14, color = C.dim))
        Row(horizontalArrangement = Arrangement.spacedBy(12.u)) {
            Button("Anuluj", Modifier.weight(1f), onClick = onDismiss)
            Button(action, Modifier.weight(1f), accent = true) { onConfirm(); onDismiss() }
        }
    }
}
