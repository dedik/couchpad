package app.couchpad.pilot.ui.theme

import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.size
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.CompositionLocalProvider
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.FilterQuality
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.painter.BitmapPainter
import androidx.compose.ui.platform.LocalConfiguration
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.res.imageResource
import androidx.compose.ui.text.TextStyle
import androidx.compose.ui.text.font.Font
import androidx.compose.ui.text.font.FontFamily
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextOverflow
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.TextUnit
import androidx.compose.ui.unit.TextUnitType
import androidx.compose.ui.unit.dp
import app.couchpad.pilot.R

/** Paleta jednego stylu (components/ui/theme.cpp). */
data class Palette(
    val bg: Color, val surface: Color, val surface2: Color, val surface3: Color, val line: Color,
    val text: Color, val dim: Color, val faint: Color,
    val accent: Color, val accentPressed: Color, val accentInk: Color, val ok: Color, val danger: Color,
    /**
     * Duże pola akcentu (OK, szerokie przyciski) jako miękkie tło z obrysem zamiast pełnego
     * wypełnienia — na OLED-zie pełny pomarańcz na dużej powierzchni razi w ciemnym pokoju.
     */
    val softAccent: Boolean = false,
    /**
     * Wypełnienia kart i kontrolek czarne jak tło — kształt rysuje wyłącznie obrys. Elementy,
     * które zwykle odróżnia od tła tylko wypełnienie, dostają wtedy obrys [C.edge], a
     * wciśnięcie rozjaśnia do `surface3` (jedyna szarość poza torami łuków).
     */
    val outlined: Boolean = false,
    /**
     * Płytka pod logotypami kanałów i aplikacji (firmware: theme::logo_plate). Ciemna także
     * w stylu jasnym: część logotypów (TVP, WP Pilot) jest biała i znika na jasnym kafelku.
     * W OLED równa kartom, żeby kafelki kanałów nie odstawały od sąsiednich.
     */
    val logoPlate: Color = Color(0xFF16181D),
)

/**
 * Ciemny styl pod OLED: tło czysto czarne (piksele gasną — głęboka czerń, mniej prądu).
 * Karty odrobinę jaśniejsze niż na płytce, żeby nie zlewały się z czernią; najciemniejsze
 * szarości na OLED-zie potrafią „smużyć” przy przewijaniu.
 */
val DarkPalette = Palette(
    bg = Color(0xFF000000), surface = Color(0xFF15171C), surface2 = Color(0xFF1E2128),
    surface3 = Color(0xFF2A2E37), line = Color(0xFF2C3039), text = Color(0xFFF3F5F9),
    dim = Color(0xFF98A0AE), faint = Color(0xFF626A78), accent = Color(0xFFFF8000),
    accentPressed = Color(0xFFFF941A), accentInk = Color(0xFF17120A), ok = Color(0xFF56D6A0),
    danger = Color(0xFFFF5F5F),
)

/**
 * Wariant ciemnego stylu tylko dla telefonów z ekranem OLED/AMOLED (strona Telefon).
 * Karty i kontrolki w środku czarne, odcięte od tła samym obrysem: piksele poza liniami,
 * tekstem i ikonami są zgaszone. Szarość zostaje na wciśnięciu i torach łuków; trzyma się
 * powyżej strefy, w której panele przy niskiej jasności zlewają ją z czernią. Tekst ok. 75%
 * bieli (mniej poświaty liter na czerni, zwłaszcza w ciemnym pokoju), a najsłabszy podpis ma kontrast ≥ 4,5:1.
 * Szarości neutralne — niebieskawy odcień przy czerni każdy panel oddaje inaczej.
 */
val OledPalette = Palette(
    bg = Color(0xFF000000), surface = Color(0xFF000000), surface2 = Color(0xFF000000),
    surface3 = Color(0xFF24272B), line = Color(0xFF2E3137), text = Color(0xFFC2C5CA),
    dim = Color(0xFF8C929B), faint = Color(0xFF777D87), accent = Color(0xFFF57C14),
    accentPressed = Color(0xFFFF9233), accentInk = Color(0xFF17120A), ok = Color(0xFF4CC892),
    danger = Color(0xFFF05A5A), softAccent = true, outlined = true,
    logoPlate = Color(0xFF000000),
)

val LightPalette = Palette(
    bg = Color(0xFFEEF0F3), surface = Color(0xFFFFFFFF), surface2 = Color(0xFFE4E7EC),
    surface3 = Color(0xFFCFD4DC), line = Color(0xFFD9DDE4), text = Color(0xFF15171C),
    dim = Color(0xFF586070), faint = Color(0xFF878F9D), accent = Color(0xFFFF8000),
    accentPressed = Color(0xFFE67300), accentInk = Color(0xFF1A1206), ok = Color(0xFF1C9A68),
    danger = Color(0xFFE03A3A),
)

/**
 * Bieżące kolory. Odczyt w kompozycji jest śledzony, więc zmiana stylu
 * (ciemny, jasny, wg godzin) przemalowuje cały interfejs bez restartu.
 */
object C {
    var palette by mutableStateOf(DarkPalette)
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
    val softAccent get() = palette.softAccent
    val outlined get() = palette.outlined

    /** Obrys elementów, które w zwykłym stylu odróżnia od tła tylko wypełnienie. */
    val edge get() = if (palette.outlined) palette.line else Color.Transparent

    /** Wypełnienie wciśniętej karty lub przycisku. */
    val pressed get() = if (palette.outlined) palette.surface3 else palette.surface2

    /** Przygaszony akcent: tło zaznaczenia (16%) i stan oczekujący (55%). */
    val accentSoft get() = palette.accent.copy(alpha = 0.16f)
    val accentPending get() = palette.accent.copy(alpha = 0.55f)

    val logoPlate get() = palette.logoPlate
}

val Ubuntu = FontFamily(
    Font(R.font.ubuntu_regular, FontWeight.Normal),
    Font(R.font.ubuntu_medium, FontWeight.Medium),
    Font(R.font.ubuntu_bold, FontWeight.Bold),
)

/**
 * Jednostka projektu: 1 piksel makiety (480 × 800 w pionie, 800 × 480 w poziomie).
 * Interfejs skaluje się proporcjonalnie do ekranu, więc na każdym telefonie wygląda jak na
 * płytce, tylko w pełnej rozdzielczości.
 */
class Units(val dpPerUnit: Float, val spPerUnit: Float)

val LocalUnits = staticCompositionLocalOf { Units(0.8f, 0.8f) }

/** Orientacja pozioma: zakładki w bocznym pasku, układy w dwóch kolumnach (ADR 0013). */
val LocalLandscape = staticCompositionLocalOf { false }

@Composable
fun ProvideUnits(content: @Composable () -> Unit) {
    val configuration = LocalConfiguration.current
    val density = LocalDensity.current
    val w = configuration.screenWidthDp.toFloat()
    val h = configuration.screenHeightDp.toFloat()
    val landscape = w > h
    // Skala dopasowana tak, żeby makieta zmieściła się w obu wymiarach; nadmiar wypełniają
    // elastyczne układy.
    val dpPerUnit = if (landscape) minOf(w / 800f, h / 480f) else minOf(w / 480f, h / 800f)
    // Tekst nie skaluje się z systemowym rozmiarem czcionki: układ jest stały jak na płytce.
    val spPerUnit = dpPerUnit / density.fontScale
    CompositionLocalProvider(
        LocalUnits provides Units(dpPerUnit, spPerUnit),
        LocalLandscape provides landscape,
        content = content,
    )
}

val Int.u: Dp @Composable get() = (this * LocalUnits.current.dpPerUnit).dp
val Float.u: Dp @Composable get() = (this * LocalUnits.current.dpPerUnit).dp
val Int.us: TextUnit @Composable get() = TextUnit(this * LocalUnits.current.spPerUnit, TextUnitType.Sp)

@Composable
fun style(size: Int, weight: FontWeight = FontWeight.Normal, color: Color = C.text) =
    TextStyle(fontFamily = Ubuntu, fontWeight = weight, fontSize = size.us, color = color)

/** Tekst w foncie projektu: jedna linia z wielokropkiem, chyba że [maxLines] mówi inaczej. */
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

/** Podpis sekcji wersalikami (URZĄDZENIA, BARWA, TRYB). */
@Composable
fun Caption(text: String, modifier: Modifier = Modifier, color: Color = C.faint) =
    T(text, 11, FontWeight.Medium, color, modifier)

/** Ikona pixel-art: skalowana bez wygładzania, żeby piksele zostały ostre. */
@Composable
fun Pixel(res: Int, size: Dp, modifier: Modifier = Modifier) = Bitmap(res, size, FilterQuality.None, modifier)

/** Logotyp z SVG: skalowany z wygładzaniem. */
@Composable
fun Logo(res: Int, size: Dp, modifier: Modifier = Modifier) = Bitmap(res, size, FilterQuality.High, modifier)

@Composable
private fun Bitmap(res: Int, size: Dp, quality: FilterQuality, modifier: Modifier) {
    val bitmap = ImageBitmap.imageResource(res)
    Image(
        painter = remember(bitmap, quality) { BitmapPainter(bitmap, filterQuality = quality) },
        contentDescription = null,
        modifier = modifier.size(size),
    )
}

/** Krótka wibracja przy dotknięciu — telefon nie ma fizycznych klawiszy pilota. */
object Haptics {
    var tick: () -> Unit = {}
}

fun haptic() = Haptics.tick()
