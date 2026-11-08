package app.couchpad.pilot.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/** Kropka 7 px w kolorze stanu. */
@Composable
fun Dot(color: Color) = Box(Modifier.size(7.u).clip(CircleShape).background(color))

/** Kropka stanu z opisem („brak połączenia”, „Optical 1”). */
@Composable
fun StatusDot(text: String, color: Color, modifier: Modifier = Modifier, size: Int = 13) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        Dot(color)
        Spacer(Modifier.width(7.u))
        T(text, size, color = C.dim)
    }
}

/** Mała pigułka informacyjna (Decode). */
@Composable
fun Pill(text: String, active: Boolean) {
    val shape = RoundedCornerShape(20.u)
    Row(
        Modifier.clip(shape).background(if (active) C.accentSoft else C.surface2)
            .border(1.u, if (active) C.accent else C.line, shape)
            .padding(horizontal = 12.u, vertical = 6.u),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Dot(if (active) C.accent else C.faint)
        Spacer(Modifier.width(6.u))
        T(text, 12, FontWeight.Medium, if (active) C.accent else C.dim)
    }
}

/** Linia oddzielająca sekcje (pasek pokoi od treści). */
@Composable
fun Divider(vertical: Int = 10) =
    Box(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = vertical.u).height(1.u).background(C.line))
