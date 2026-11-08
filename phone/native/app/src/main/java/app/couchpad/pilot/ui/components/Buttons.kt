package app.couchpad.pilot.ui.components

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.interaction.collectIsPressedAsState
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.PowerSettingsNew
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/** Karta z obrysem i reakcją na wciśnięcie; zaznaczona ma obrys w akcencie. */
@Composable
fun Card(
    modifier: Modifier = Modifier,
    radius: Int = 20,
    selected: Boolean = false,
    onClick: (() -> Unit)? = null,
    content: @Composable () -> Unit,
) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()
    val shape = RoundedCornerShape(radius.u)
    Box(
        modifier
            .clip(shape)
            .background(if (pressed) C.surface2 else C.surface)
            .border(1.u, if (selected) C.accent else C.line, shape)
            .then(if (onClick != null) Modifier.tap(interaction = interaction, onClick = onClick) else Modifier),
    ) { content() }
}

/** Przycisk z ikoną i tekstem: zwykły, akcentowany albo zaznaczony. */
@Composable
fun Button(
    label: String,
    modifier: Modifier = Modifier,
    icon: ImageVector? = null,
    accent: Boolean = false,
    selected: Boolean = false,
    pending: Boolean = false,
    height: Int = 56,
    radius: Int = 16,
    textSize: Int = 15,
    onClick: () -> Unit,
) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()
    val shape = RoundedCornerShape(radius.u)
    val bg = when {
        accent -> if (pressed) C.accentPressed else C.accent
        selected -> if (pressed) C.surface3 else C.surface2
        else -> if (pressed) C.surface2 else C.surface
    }
    val border = when {
        selected -> C.accent.copy(alpha = if (pending) 0.4f else 1f)
        accent -> Color.Transparent
        else -> C.line
    }
    val fg = if (accent) C.accentInk else C.text
    Box(
        modifier
            .clip(shape)
            .background(bg)
            .border(1.u, border, shape)
            .tap(interaction = interaction, onClick = onClick)
            .then(if (height > 0) Modifier.height(height.u) else Modifier)
            .padding(horizontal = 12.u),
        contentAlignment = Alignment.Center,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.Center) {
            if (icon != null) {
                Icon(icon, null, tint = fg, modifier = Modifier.size(20.u))
                if (label.isNotEmpty()) Spacer(Modifier.width(8.u))
            }
            if (label.isNotEmpty()) T(label, textSize, FontWeight.Medium, fg)
        }
    }
}

/** Okrągły przycisk z ikoną (zasilanie, −/+, wstecz). */
@Composable
fun RoundButton(
    icon: ImageVector,
    modifier: Modifier = Modifier,
    size: Int = 48,
    tint: Color = C.text,
    background: Color = C.surface2,
    border: Color = C.line,
    shape: Shape = CircleShape,
    onClick: () -> Unit,
) {
    val interaction = remember { MutableInteractionSource() }
    val pressed by interaction.collectIsPressedAsState()
    Box(
        modifier
            .size(size.u)
            .clip(shape)
            .background(if (pressed) C.surface3 else background)
            .border(1.u, border, shape)
            .tap(interaction = interaction, onClick = onClick),
        contentAlignment = Alignment.Center,
    ) {
        Icon(icon, null, tint = tint, modifier = Modifier.size((size * 0.45f).u))
    }
}

@Composable
fun BackButton(onClick: () -> Unit) = RoundButton(Icons.Rounded.ArrowBackIosNew, size = 44, onClick = onClick)

/**
 * Wygląd zasilania (tv-and-z906-ui.md): [Solid] w nagłówku urządzenia — wyłączone szare;
 * [Soft] w poziomach Domu i klimie — wyłączone na bursztynowym tle, żeby było widać skrót.
 */
enum class PowerStyle { Solid, Soft }

/** Zasilanie: włączone wypełnione akcentem, oczekujące z obrysem w akcencie. */
@Composable
fun PowerButton(
    on: Boolean,
    modifier: Modifier = Modifier,
    size: Int = 44,
    pending: Boolean = false,
    style: PowerStyle = PowerStyle.Soft,
    onToggle: (Boolean) -> Unit,
) {
    val soft = style == PowerStyle.Soft
    RoundButton(
        Icons.Rounded.PowerSettingsNew,
        modifier,
        size = size,
        tint = when {
            on -> C.accentInk
            soft -> C.accent
            else -> C.dim
        },
        background = when {
            on -> C.accent
            soft -> C.accent.copy(alpha = 0.14f)
            else -> C.surface2
        },
        border = if (on || pending) C.accent else C.line,
    ) { onToggle(!on) }
}
