package app.couchpad.pilot.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/**
 * Nagłówek ekranu urządzenia: ikona, nazwa, pokój, opcjonalna treść (w poziomie wybór
 * źródła i trybu) i zasilanie. Bez [onPower] przycisku nie ma.
 */
@Composable
fun DeviceHeader(
    icon: Int,
    title: String,
    subtitle: String,
    power: Boolean? = null,
    powerPending: Boolean = false,
    extra: @Composable RowScope.() -> Unit = {},
    onPower: ((Boolean) -> Unit)? = null,
) {
    Row(
        Modifier.fillMaxWidth().padding(start = 24.u, end = 24.u, top = 10.u, bottom = 6.u).height(56.u),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(40.u).clip(RoundedCornerShape(12.u)).background(C.surface2), contentAlignment = Alignment.Center) {
            Pixel(icon, 30.u)
        }
        Spacer(Modifier.width(12.u))
        Column(Modifier.weight(1f)) {
            T(title, 19, FontWeight.Bold)
            Caption(subtitle.uppercase())
        }
        extra()
        if (onPower != null) {
            Spacer(Modifier.width(10.u))
            PowerButton(power == true, pending = powerPending, style = PowerStyle.Solid, onToggle = onPower)
        }
    }
}

/**
 * Nagłówek podstrony: wstecz, tytuł z podpisem i treść po prawej (np. zasilanie).
 * Bez [onBack] (szczegół ustawień w poziomie) przycisku wstecz nie ma.
 */
@Composable
fun PageHeader(
    title: String,
    caption: String? = null,
    onBack: (() -> Unit)? = null,
    trailing: @Composable RowScope.() -> Unit = {},
) {
    Row(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 10.u), verticalAlignment = Alignment.CenterVertically) {
        if (onBack != null) {
            BackButton(onBack)
            Spacer(Modifier.width(14.u))
        }
        Column(Modifier.weight(1f)) {
            T(title, 20, FontWeight.Bold)
            if (caption != null) Caption(caption)
        }
        trailing()
    }
}
