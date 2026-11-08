package app.couchpad.pilot.ui.components

import android.os.Build
import android.graphics.drawable.ColorDrawable
import android.graphics.drawable.LayerDrawable
import android.graphics.drawable.Drawable
import android.view.Gravity
import android.view.Window
import android.view.WindowManager
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.lazy.LazyColumn
import androidx.compose.foundation.lazy.items
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import kotlin.math.roundToInt
import androidx.compose.ui.layout.positionInWindow
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.runtime.getValue
import androidx.compose.runtime.SideEffect
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.window.Dialog
import androidx.compose.ui.window.DialogWindowProvider
import androidx.compose.ui.window.DialogProperties
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.ProvideUnits
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/** Opcja listy wyboru: identyfikator wysyłany w poleceniu i podpis. */
data class Choice(val id: String, val label: String)

fun List<String>.asChoices(label: (String) -> String = { it }) = map { Choice(it, label(it)) }

/**
 * Lista wyboru (home-screen.md §4.6): w pionie pełny ekran z nagłówkiem, w poziomie panel
 * 380 px przy prawej krawędzi nad przyciemnionym ekranem. Każda opcja to karta z ikoną,
 * bieżąca ma obwódkę w akcencie i podpis „aktywny”. **Wybór zamyka listę**; Wstecz albo
 * dotknięcie tła zamyka bez zmian.
 */
@Composable
fun OptionDialog(
    title: String,
    options: List<Choice>,
    selected: String?,
    onDismiss: () -> Unit,
    icon: ((String) -> ImageVector)? = null,
    onSelect: (String) -> Unit,
) {
    Dialog(
        onDismissRequest = onDismiss,
        properties = DialogProperties(usePlatformDefaultWidth = false, decorFitsSystemWindows = false),
    ) {
        val window = (LocalView.current.parent as? DialogWindowProvider)?.window
        // Lewy brzeg panelu w oknie (poziom); do niego sięga jasny pas tła okna.
        var panelLeft by remember { mutableStateOf<Int?>(null) }
        ProvideUnits {
            if (window != null) {
                val landscape = LocalLandscape.current
                val bg = C.bg.toArgb()
                SideEffect { fillScreen(window, landscape, panelLeft, bg) }
            }
            val list: @Composable (Modifier) -> Unit = { modifier ->
                Column(modifier.background(C.bg)) {
                    PageHeader(title, onBack = onDismiss)
                    LazyColumn(
                        Modifier.fillMaxWidth().weight(1f),
                        contentPadding = PaddingValues(start = 24.u, end = 24.u, bottom = 24.u),
                        verticalArrangement = Arrangement.spacedBy(12.u),
                    ) {
                        items(options) { option ->
                            ChoiceCard(option, option.id == selected, icon?.invoke(option.id)) {
                                onSelect(option.id)
                                onDismiss()
                            }
                        }
                    }
                }
            }
            if (LocalLandscape.current) {
                Row(Modifier.fillMaxSize()) {
                    // Przyciemnia tło okna (fillScreen) — razem z pasem pod paskiem nawigacji.
                    Box(Modifier.weight(1f).fillMaxHeight().tap(feedback = false, onClick = onDismiss))
                    list(
                        Modifier.width(380.u).fillMaxHeight().background(C.bg)
                            .onGloballyPositioned { panelLeft = it.positionInWindow().x.roundToInt() }
                            .systemBarsPadding(),
                    )
                }
            } else {
                // Tło przed odstępem od pasków systemu, żeby pod paskiem statusu nie prześwitywało
                // przyciemnienie okna dialogu (szary pas w stylu jasnym).
                list(Modifier.fillMaxSize().background(C.bg).systemBarsPadding())
            }
        }
    }
}

@Composable
private fun ChoiceCard(option: Choice, active: Boolean, icon: ImageVector?, onClick: () -> Unit) {
    val shape = RoundedCornerShape(16.u)
    Row(
        Modifier.fillMaxWidth().height(76.u).clip(shape).background(C.surface)
            .border(1.u, if (active) C.accent else C.line, shape)
            .tap(onClick = onClick)
            .padding(horizontal = 20.u),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) {
            Icon(icon, null, tint = if (active) C.accent else C.dim, modifier = Modifier.size(24.u))
            Spacer(Modifier.width(16.u))
        }
        T(option.label, 15, FontWeight.Medium, C.text, Modifier.weight(1f))
        if (active) T("aktywny", 12, color = C.faint)
    }
}

/**
 * Przycisk wyboru (widgets::make_value_segment): mała ikona z lewej, obok bieżąca opcja
 * jako przycisk otwierający listę. Ikony jak na płytce: źródło — błyskawica, obraz — obrazek,
 * wejście Z906 — USB, efekt — nuta.
 */
@Composable
fun SelectorButton(
    value: String,
    modifier: Modifier = Modifier,
    pending: Boolean = false,
    icon: ImageVector? = null,
    onClick: () -> Unit,
) {
    Row(modifier, verticalAlignment = Alignment.CenterVertically) {
        if (icon != null) {
            Icon(icon, null, tint = C.faint, modifier = Modifier.width(24.u).size(20.u))
            Spacer(Modifier.width(10.u))
        }
        Button(
            value.ifEmpty { "—" }, Modifier.weight(1f), selected = value.isNotEmpty(), pending = pending,
            height = 52, textSize = 14, onClick = onClick,
        )
    }
}

/** Przełącznik segmentowy: jedna opcja z kilku, jak na płytce. */
@Composable
fun Segmented(labels: List<String>, selected: Int, onSelect: (Int) -> Unit) {
    Row(
        Modifier.fillMaxWidth().height(48.u).clip(RoundedCornerShape(14.u)).background(C.surface)
            .border(1.u, C.line, RoundedCornerShape(14.u)).padding(4.u),
    ) {
        labels.forEachIndexed { i, label ->
            val active = i == selected
            val shape = RoundedCornerShape(10.u)
            Box(
                Modifier.weight(1f).fillMaxHeight().clip(shape)
                    .background(if (active) C.surface2 else Color.Transparent)
                    .border(1.u, if (active) C.accent else Color.Transparent, shape)
                    .tap { onSelect(i) },
                contentAlignment = Alignment.Center,
            ) {
                T(label, 13, if (active) FontWeight.Bold else FontWeight.Normal, if (active) C.text else C.dim)
            }
        }
    }
}

/** Kafel opcji (home-screen.md §4.4): podpis wersalikami nad wartością, opcjonalna ikona. */
@Composable
fun OptionTile(
    caption: String,
    value: String,
    modifier: Modifier = Modifier,
    active: Boolean = false,
    pending: Boolean = false,
    icon: (@Composable () -> Unit)? = null,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(14.u)
    Box(
        modifier.clip(shape)
            .background(if (active) C.accentSoft else C.surface2)
            .then(if (active || pending) Modifier.border(1.u, pendingBorder(active, pending), shape) else Modifier)
            .tap(onClick = onClick)
            .padding(horizontal = 14.u, vertical = 10.u),
        contentAlignment = Alignment.CenterStart,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            if (icon != null) {
                icon()
                Spacer(Modifier.width(12.u))
            }
            Column(Modifier.weight(1f)) {
                T(caption, 10, FontWeight.Medium, if (active) C.accent else C.faint)
                Spacer(Modifier.height(4.u))
                T(value, 15, FontWeight.Medium, if (active) C.accent else C.text)
            }
        }
    }
}

/** Obrys kafla: pełny akcent, gdy aktywny; przygaszony, gdy tylko czeka na potwierdzenie. */
fun pendingBorder(active: Boolean, pending: Boolean): Color =
    C.accent.copy(alpha = if (pending && !active) 0.5f else 1f)

/**
 * Okno listy wyboru na cały ekran, jak pilot (MainActivity): pasek stanu ukryty, paski
 * przezroczyste, bez systemowego przyciemnienia (w stylu jasnym zostawało szarym pasem pod
 * paskiem stanu). Compose 1.5 i tak mierzy treść dialogu bez paska nawigacji, więc pas pod
 * nim maluje tło okna: w pionie tłem listy, w poziomie przyciemnieniem z jasnym pasem od
 * lewego brzegu panelu ([panelLeft]) do prawej krawędzi.
 */
private fun fillScreen(window: Window, landscape: Boolean, panelLeft: Int?, bg: Int) {
    window.setDimAmount(0f)
    val metrics = android.util.DisplayMetrics()
    @Suppress("DEPRECATION")
    window.windowManager.defaultDisplay.getRealMetrics(metrics)
    window.setLayout(metrics.widthPixels, metrics.heightPixels)
    window.setGravity(Gravity.TOP or Gravity.START)
    window.addFlags(WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS)
    window.attributes = window.attributes.apply {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) fitInsetsTypes = 0
    }
    window.statusBarColor = android.graphics.Color.TRANSPARENT
    window.navigationBarColor = android.graphics.Color.TRANSPARENT
    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) window.isNavigationBarContrastEnforced = false
    WindowCompat.setDecorFitsSystemWindows(window, false)
    WindowInsetsControllerCompat(window, window.decorView).apply {
        hide(WindowInsetsCompat.Type.statusBars())
        systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
    }
    window.setBackgroundDrawable(background(window, landscape, panelLeft, bg))
}

/**
 * Tło okna listy. W poziomie jasny pas od [panelLeft] do prawej krawędzi ekranu, a na lewo
 * przyciemnienie; dopóki panel nie jest rozmieszczony — samo przyciemnienie.
 */
private fun background(window: Window, landscape: Boolean, panelLeft: Int?, bg: Int): Drawable {
    if (!landscape) return ColorDrawable(bg)
    val width = window.decorView.width
    if (panelLeft == null || width <= 0) return ColorDrawable(SCRIM)
    return LayerDrawable(arrayOf(ColorDrawable(SCRIM), ColorDrawable(bg))).apply {
        setLayerGravity(1, Gravity.END or Gravity.FILL_VERTICAL)
        setLayerWidth(1, (width - panelLeft).coerceAtLeast(0))
    }
}

/** Przyciemnienie ekranu pod panelem listy w poziomie (czarny, 60%). */
private const val SCRIM = 0x99000000.toInt()
