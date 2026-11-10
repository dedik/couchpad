package app.couchpad.pilot.ui.components

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.systemBars
import androidx.compose.foundation.layout.windowInsetsPadding
import androidx.compose.foundation.layout.union
import androidx.compose.foundation.layout.only
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.WindowInsetsSides
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.PaddingValues
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.displayCutoutPadding
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
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.SideEffect
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.staticCompositionLocalOf
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Pixel
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
 *
 * Rysuje się w warstwie nad ekranem pilota ([LocalOverlay]), nie w osobnym oknie dialogu:
 * okno dialogu w Compose 1.5 nie sięga pod paski systemu, więc lista otwierała się z pasami
 * przy krawędziach i przeskakiwała w pierwszej klatce.
 */
@Composable
fun OptionDialog(
    title: String,
    options: List<Choice>,
    selected: String?,
    onDismiss: () -> Unit,
    icon: ((String) -> Int)? = null,
    onSelect: (String) -> Unit,
) {
    BackHandler(onBack = onDismiss)
    val sheet: @Composable () -> Unit = { OptionSheet(title, options, selected, onDismiss, icon, onSelect) }
    val overlay = LocalOverlay.current
    if (overlay == null) {
        sheet()
        return
    }
    // Warstwa dostaje zawsze najnowszą treść; znika razem z listą.
    val key = remember { Any() }
    SideEffect { overlay.show(key, sheet) }
    DisposableEffect(overlay, key) { onDispose { overlay.hide(key) } }
}

@Composable
private fun OptionSheet(
    title: String,
    options: List<Choice>,
    selected: String?,
    onDismiss: () -> Unit,
    icon: ((String) -> Int)?,
    onSelect: (String) -> Unit,
) {
    val list: @Composable (Modifier) -> Unit = { modifier ->
        Column(modifier) {
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
            Box(Modifier.weight(1f).fillMaxHeight().background(Color.Black.copy(alpha = 0.6f)).tap(feedback = false, onClick = onDismiss))
            // Tło do samej krawędzi, treść z odstępem od pasków systemu.
            list(
                Modifier.width(380.u).fillMaxHeight().background(C.bg).windowInsetsPadding(
                    // Panel stoi przy prawej krawędzi: wcięcie kamery z lewej go nie dotyczy.
                    WindowInsets.systemBars.union(WindowInsets.displayCutout)
                        .only(WindowInsetsSides.Vertical + WindowInsetsSides.End),
                ),
            )
        }
    } else {
        // Pełny ekran zabiera też dotyk, żeby nie przechodził do ekranu pod spodem.
        list(Modifier.fillMaxSize().background(C.bg).tap(feedback = false) {}.systemBarsPadding().displayCutoutPadding())
    }
}

/**
 * Warstwa nad całym ekranem pilota (PilotApp): tu rysują się listy wyboru. Ostatnio
 * pokazana jest na wierzchu.
 */
class Overlay {
    private val entries = mutableStateListOf<Pair<Any, @Composable () -> Unit>>()

    fun show(key: Any, content: @Composable () -> Unit) {
        val i = entries.indexOfFirst { it.first === key }
        if (i >= 0) entries[i] = key to content else entries.add(key to content)
    }

    fun hide(key: Any) {
        entries.removeAll { it.first === key }
    }

    @Composable
    fun Content() {
        for ((key, content) in entries) androidx.compose.runtime.key(key) { content() }
    }
}

val LocalOverlay = staticCompositionLocalOf<Overlay?> { null }

@Composable
private fun ChoiceCard(option: Choice, active: Boolean, icon: Int?, onClick: () -> Unit) {
    val shape = RoundedCornerShape(16.u)
    Row(
        Modifier.fillMaxWidth().height(76.u).clip(shape).background(C.surface)
            .border(1.u, if (active) C.accent else C.line, shape)
            .tap(onClick = onClick)
            .padding(horizontal = 20.u),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        if (icon != null) {
            // Pixel art (efekty lampy) w 1 x, bez wygładzania.
            Pixel(icon, 24.u)
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
            .border(1.u, if (active || pending) pendingBorder(active, pending) else C.edge, shape)
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
