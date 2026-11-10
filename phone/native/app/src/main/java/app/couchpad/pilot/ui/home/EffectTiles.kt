package app.couchpad.pilot.ui.home

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.EffectUsage
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/**
 * Efekty lampy jako kafle, jak tryby klimatyzacji (tylko aplikacja; panel ma wiersz z listą).
 * Bieżący efekt na dużym kaflu po lewej; obok najczęściej wybierane efekty tej lampy, a gdy
 * efekt jest włączony, pierwszy z nich to zawsze „Brak”. Ostatni kafel „Więcej” otwiera pełną
 * listę. W pionie obok mieszczą się 4 kafle (3 + Więcej), w poziomie 5 (4 + Więcej na całą
 * wysokość), więc całość nie wymaga przewijania.
 */
@Composable
internal fun EffectTiles(
    effects: List<String>,
    current: String,
    usage: Map<String, Int>,
    pending: Boolean,
    onSelect: (String) -> Unit,
    onMore: () -> Unit,
) {
    val landscape = LocalLandscape.current
    val slots = if (landscape) 4 else 3
    val active = current.isNotEmpty()
    // Ile innych efektów mieści się obok; „Brak” zajmuje jedno miejsce, gdy efekt jest włączony.
    val room = if (active) slots - 1 else slots
    val frequent = EffectUsage.frequent(effects, usage, current, room)
    val small = (if (active) listOf("") else emptyList()) + frequent
    val hidden = effects.count { it.isNotEmpty() && it != current } - frequent.size
    val gap = 8.u

    val tile: @Composable (String, Boolean, Modifier) -> Unit = { id, large, modifier ->
        EffectTile(
            Effects.icon(id), if (id.isEmpty()) "Brak" else Effects.name(id), large,
            selected = id.isNotEmpty() && id == current, pending = pending && id == current, modifier,
        ) { if (id != current) onSelect(id) }
    }
    val more: @Composable (Modifier) -> Unit = { modifier ->
        if (hidden > 0) {
            EffectTile(R.drawable.px_fx_more, "Więcej · $hidden", large = false, selected = false, pending = false, modifier, onClick = onMore)
        } else {
            Spacer(modifier)
        }
    }
    val cell: @Composable (Int, Modifier) -> Unit = { i, modifier ->
        small.getOrNull(i)?.let { tile(it, false, modifier) } ?: Spacer(modifier)
    }

    Row(Modifier.fillMaxWidth().height(if (landscape) 136.u else 152.u), horizontalArrangement = Arrangement.spacedBy(gap)) {
        tile(current, true, Modifier.weight(if (landscape) 1.5f else 6f / 5f).fillMaxHeight())
        if (landscape) {
            // [duży][0][1][Więcej] / [duży][2][3][Więcej]
            for (col in 0 until 2) {
                Column(Modifier.weight(1f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(gap)) {
                    cell(col, Modifier.fillMaxWidth().weight(1f))
                    cell(col + 2, Modifier.fillMaxWidth().weight(1f))
                }
            }
            more(Modifier.weight(1f).fillMaxHeight())
        } else {
            // [duży][0][1] / [duży][2][Więcej]
            Column(Modifier.weight(2f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(gap)) {
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    cell(0, Modifier.weight(1f).fillMaxHeight())
                    cell(1, Modifier.weight(1f).fillMaxHeight())
                }
                Row(Modifier.fillMaxWidth().weight(1f), horizontalArrangement = Arrangement.spacedBy(gap)) {
                    cell(2, Modifier.weight(1f).fillMaxHeight())
                    more(Modifier.weight(1f).fillMaxHeight())
                }
            }
        }
    }
}

/** Kafel efektu: ikona nad nazwą; wybrany na miękkim bursztynie z obwódką (jak ModeTile klimy). */
@Composable
private fun EffectTile(
    icon: Int,
    label: String,
    large: Boolean,
    selected: Boolean,
    pending: Boolean,
    modifier: Modifier,
    onClick: () -> Unit,
) {
    val shape = RoundedCornerShape(14.u)
    val ink = if (selected) C.accent else C.dim
    Column(
        modifier
            .clip(shape)
            .background(if (selected) C.accentSoft else C.surface2)
            .then(
                when {
                    pending -> Modifier.border(1.u, C.accentPending, shape)
                    selected -> Modifier.border(1.u, C.accent, shape)
                    else -> Modifier.border(1.u, C.edge, shape)
                },
            )
            .tap(onClick = onClick)
            .padding(horizontal = 6.u),
        verticalArrangement = Arrangement.spacedBy(if (large) 8.u else 6.u, Alignment.CenterVertically),
        horizontalAlignment = Alignment.CenterHorizontally,
    ) {
        // Pixel art 24 x 24: mały kafel 1 x, duży 2 x — bez wygładzania.
        Pixel(icon, if (large) 48.u else 24.u)
        T(label, if (large) 15 else 13, if (large) FontWeight.Medium else FontWeight.Normal, ink, maxLines = 1)
    }
}
