package app.couchpad.pilot.ui

import androidx.compose.foundation.Image
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.clickable
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
import androidx.compose.material3.Icon
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.Shape
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.res.imageResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.dp
import app.couchpad.pilot.R

/** Paleta jednego stylu (components/ui/theme.cpp). */
data class Palette(
    val bg: Color, val surface: Color, val surface2: Color, val surface3: Color, val line: Color,
    val text: Color, val dim: Color, val faint: Color,
    val accent: Color, val accentPressed: Color, val accentInk: Color, val ok: Color, val danger: Color,
)

val DarkPalette = Palette(
    bg = Color(0xFF0A0B0E), surface = Color(0xFF14161B), surface2 = Color(0xFF1C1F26),
    surface3 = Color(0xFF282C35), line = Color(0xFF30353F), text = Color(0xFFF3F5F9),
    dim = Color(0xFF98A0AE), faint = Color(0xFF626A78), accent = Color(0xFFF2A93B),
    accentPressed = Color(0xFFF4B25A), accentInk = Color(0xFF17120A), ok = Color(0xFF56D6A0),
    danger = Color(0xFFFF5F5F),
)

val LightPalette = Palette(
    bg = Color(0xFFEEF0F3), surface = Color(0xFFFFFFFF), surface2 = Color(0xFFE4E7EC),
    surface3 = Color(0xFFCFD4DC), line = Color(0xFFD9DDE4), text = Color(0xFF15171C),
    dim = Color(0xFF586070), faint = Color(0xFF878F9D), accent = Color(0xFFE08A12),
    accentPressed = Color(0xFFB86F0A), accentInk = Color(0xFF1A1206), ok = Color(0xFF1C9A68),
    danger = Color(0xFFE03A3A),
)

/**
 * Bieżące kolory. Odczyt w kompozycji jest śledzony, więc zmiana stylu
 * (ciemny, jasny, wg godzin) przemalowuje cały interfejs bez restartu.
 */
object C {
    var palette by androidx.compose.runtime.mutableStateOf(DarkPalette)
    val bg get() = palette.bg
    val surface get() = palette.surface
    val surface2 get() = palette.surface2
    val surface3 get() = palette.surface3
    val line get() = palette.line
    val text get() = palette.text
    val dim get() = palette.dim
    val faint get() = palette.faint
    val accent get() = palette.accent
    val accentPressed get() = palette.accentPressed
    val accentInk get() = palette.accentInk
    val ok get() = palette.ok
    val danger get() = palette.danger
}

val Ubuntu = FontFamily(
    Font(R.font.ubuntu_regular, FontWeight.Normal),
    Font(R.font.ubuntu_medium, FontWeight.Medium),
    Font(R.font.ubuntu_bold, FontWeight.Bold),
)

/**
 * Jednostka projektu: 1 piksel makiety 480 px szerokości. Interfejs skaluje się
 * proporcjonalnie do szerokości ekranu, więc na każdym telefonie wygląda jak na płytce,
 * tylko w pełnej rozdzielczości.
 */
class Units(val dpPerUnit: Float, val spPerUnit: Float)

val LocalUnits = staticCompositionLocalOf { Units(0.8f, 0.8f) }

@Composable
fun ProvideUnits(content: @Composable () -> Unit) {
    val configuration = LocalConfiguration.current
    val density = LocalDensity.current
    val w = configuration.screenWidthDp.toFloat()
    val h = configuration.screenHeightDp.toFloat()
    val landscape = w > h
    // Makieta: 480 × 800 w pionie, 800 × 480 w poziomie (ADR 0013). Skala dopasowana tak,
    // żeby makieta zmieściła się w obu wymiarach; nadmiar wypełniają elastyczne układy.
    val dpPerUnit = if (landscape) minOf(w / 800f, h / 480f) else minOf(w / 480f, h / 800f)
    // Tekst nie skaluje się z systemowym rozmiarem czcionki: układ jest stały jak na płytce.
    val spPerUnit = dpPerUnit / density.fontScale
    CompositionLocalProvider(
        LocalUnits provides Units(dpPerUnit, spPerUnit),
        LocalLandscape provides landscape,
        content = content,
    )
}

/** Orientacja pozioma: zakładki w bocznym pasku, układy w dwóch kolumnach. */
val LocalLandscape = staticCompositionLocalOf { false }

val Int.u: Dp @Composable get() = (this * LocalUnits.current.dpPerUnit).dp
val Float.u: Dp @Composable get() = (this * LocalUnits.current.dpPerUnit).dp
val Int.us: TextUnit @Composable get() = androidx.compose.ui.unit.TextUnit(this * LocalUnits.current.spPerUnit, androidx.compose.ui.unit.TextUnitType.Sp)

@Composable
fun style(size: Int, weight: FontWeight = FontWeight.Normal, color: Color = C.text) =
    TextStyle(fontFamily = Ubuntu, fontWeight = weight, fontSize = size.us, color = color)

@Composable
fun T(
    text: String,
    size: Int,
    weight: FontWeight = FontWeight.Normal,
    color: Color = C.text,
    modifier: Modifier = Modifier,
    maxLines: Int = 1,
) = Text(
    text,
    style = style(size, weight, color),
    modifier = modifier,
    maxLines = maxLines,
    overflow = TextOverflow.Ellipsis,
)

/** Ikona pixel-art: skalowana bez wygładzania, żeby piksele zostały ostre. */
@Composable
fun Pixel(res: Int, size: Dp, modifier: Modifier = Modifier) {
    val bitmap = ImageBitmap.imageResource(res)
    Image(
        painter = remember(bitmap) { BitmapPainter(bitmap, filterQuality = FilterQuality.None) },
        contentDescription = null,
        modifier = modifier.size(size),
    )
}

@Composable
fun Logo(res: Int, size: Dp, modifier: Modifier = Modifier) {
    val bitmap = ImageBitmap.imageResource(res)
    Image(
        painter = remember(bitmap) { BitmapPainter(bitmap, filterQuality = FilterQuality.High) },
        contentDescription = null,
        modifier = modifier.size(size),
    )
}

/** Karta z obrysem i reakcją na wciśnięcie. */
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
    var m = modifier
        .clip(shape)
        .background(if (pressed) C.surface2 else C.surface)
        .border(1.u, if (selected) C.accent else C.line, shape)
    if (onClick != null) {
        m = m.clickable(interactionSource = interaction, indication = null) {
            haptic()
            onClick()
        }
    }
    Box(m) { content() }
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
    val fg = if (accent) C.accentInk else C.text
    Box(
        modifier
            .clip(shape)
            .background(bg)
            .border(1.u, if (selected) C.accent.copy(alpha = if (pending) 0.4f else 1f) else if (accent) Color.Transparent else C.line, shape)
            .clickable(interactionSource = interaction, indication = null) {
                haptic()
                onClick()
            }
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

/** Okrągły przycisk z ikoną (zasilanie, −/+). */
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
            .clickable(interactionSource = interaction, indication = null) {
                haptic()
                onClick()
            },
        contentAlignment = Alignment.Center,
    ) {
        Icon(icon, null, tint = tint, modifier = Modifier.size((size * 0.45f).u))
    }
}

/** Kropka stanu z opisem („brak połączenia”, „Optical 1”). */
@Composable
fun StatusDot(text: String, color: Color, modifier: Modifier = Modifier, size: Int = 13) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        Box(Modifier.size(7.u).clip(CircleShape).background(color))
        Spacer(Modifier.width(7.u))
        T(text, size, color = C.dim)
    }
}

/** Krótka wibracja przy dotknięciu — telefon nie ma fizycznych klawiszy pilota. */
var haptic: () -> Unit = {}
