package app.couchpad.pilot.ui.settings

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.State
import androidx.compose.runtime.produceState
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.KeyboardType
import androidx.compose.ui.text.input.PasswordVisualTransformation
import androidx.compose.ui.text.input.VisualTransformation
import app.couchpad.pilot.ui.components.Segmented
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.style
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.delay
import kotlinx.coroutines.withContext
import java.net.Inet4Address
import java.net.NetworkInterface

/** Grupa ustawienia: podpis wersalikami nad kontrolką. */
@Composable
internal fun Group(title: String, content: @Composable () -> Unit) {
    Column(verticalArrangement = Arrangement.spacedBy(8.u)) {
        Caption(title.uppercase())
        content()
    }
}

/** Grupa z przełącznikiem segmentowym — najczęstsze ustawienie. */
@Composable
internal fun Choose(title: String, labels: List<String>, selected: Int, onSelect: (Int) -> Unit) =
    Group(title) { Segmented(labels, selected, onSelect) }

@Composable
internal fun Note(text: String) = Text(text, style = style(12, color = C.faint))

/** Wiersz informacji: etykieta i wartość; [ok] na zielono. */
@Composable
internal fun InfoRow(label: String, value: String, ok: Boolean = false) {
    Row(Modifier.fillMaxWidth(), verticalAlignment = Alignment.CenterVertically) {
        T(label, 15, FontWeight.Medium, modifier = Modifier.weight(1f))
        T(value, 13, color = if (ok) C.ok else C.dim)
    }
}

@Composable
internal fun Field(
    label: String,
    value: String,
    onChange: (String) -> Unit,
    keyboard: KeyboardType = KeyboardType.Text,
    password: Boolean = false,
) {
    val shape = RoundedCornerShape(12.u)
    Column {
        T(label, 12, color = C.dim)
        Spacer(Modifier.height(6.u))
        BasicTextField(
            value = value,
            onValueChange = onChange,
            singleLine = true,
            textStyle = style(16),
            cursorBrush = SolidColor(C.accent),
            visualTransformation = if (password) PasswordVisualTransformation() else VisualTransformation.None,
            keyboardOptions = KeyboardOptions(keyboardType = if (password) KeyboardType.Password else keyboard, autoCorrect = false),
            modifier = Modifier.fillMaxWidth().clip(shape).background(C.surface2).border(1.u, C.line, shape)
                .padding(horizontal = 14.u, vertical = 12.u),
        )
    }
}

/**
 * Adres IPv4 telefonu w sieci lokalnej, odświeżany co kilka sekund poza wątkiem UI
 * (wyliczanie interfejsów sieciowych nie jest darmowe).
 */
@Composable
internal fun rememberLocalIp(): State<String?> = produceState<String?>(null) {
    while (true) {
        value = withContext(Dispatchers.IO) { localIp() }
        delay(5_000)
    }
}

private fun localIp(): String? = runCatching {
    NetworkInterface.getNetworkInterfaces().toList()
        .filter { it.isUp && !it.isLoopback }
        .flatMap { it.inetAddresses.toList() }
        .firstOrNull { it is Inet4Address && !it.isLoopbackAddress }?.hostAddress
}.getOrNull()
