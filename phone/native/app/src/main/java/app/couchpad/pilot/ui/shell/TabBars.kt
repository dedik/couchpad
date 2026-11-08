package app.couchpad.pilot.ui.shell

import android.graphics.Rect
import android.os.Build
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.layout.positionInWindow
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.u

/** Wycięcie na kamerę przy lewej krawędzi (poziom): prostokąt w pikselach okna albo null. */
@Composable
private fun leftCutout(): Rect? {
    val view = LocalView.current
    val left = WindowInsets.displayCutout.getLeft(LocalDensity.current, LayoutDirection.Ltr)
    return remember(left, view) {
        if (left == 0 || Build.VERSION.SDK_INT < 28) null
        else view.rootWindowInsets?.displayCutout?.boundingRects?.firstOrNull { it.left <= 0 && it.width() > 0 }
    }
}

/** Szerokość wycięcia na kamerę przy lewej krawędzi; 0, gdy go nie ma. */
@Composable
fun cutoutWidth(): Dp = with(LocalDensity.current) { (leftCutout()?.right ?: 0).toDp() }

/** Kafel zakładki: ikona pixel-art, wybrana na bursztynowym tle. */
@Composable
private fun TabIcon(tab: Screen, selected: Boolean, iconSize: Int, modifier: Modifier, onSelect: (Screen) -> Unit) {
    Box(
        modifier
            .clip(RoundedCornerShape(16.u))
            .background(if (selected) C.accentSoft else Color.Transparent)
            .tap { onSelect(tab) },
        contentAlignment = Alignment.Center,
    ) { Pixel(tab.icon, iconSize.u) }
}

/**
 * Boczny pasek zakładek (poziom, ADR 0013). Ikony stoją przy samej krawędzi wyświetlacza,
 * także w pasie obok przedniej kamery; tylko ikona, która wypada na wysokości kamery, jest
 * odsunięta o szerokość wycięcia.
 */
@Composable
fun SideRail(current: Screen, onSelect: (Screen) -> Unit) {
    val cut = leftCutout()
    val cutWidth = cutoutWidth()
    Column(
        Modifier.fillMaxHeight().width(80.u + cutWidth).background(C.bg).padding(vertical = 10.u),
        verticalArrangement = Arrangement.SpaceEvenly,
    ) {
        for (tab in Screen.tabs) {
            var top by remember { mutableStateOf(0f) }
            var bottom by remember { mutableStateOf(0f) }
            val atCamera = cut != null && bottom > cut.top && top < cut.bottom
            Box(
                Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .onGloballyPositioned { c ->
                        top = c.positionInWindow().y
                        bottom = top + c.size.height
                    }
                    .padding(start = if (atCamera) cutWidth + 8.u else 8.u, top = 4.u, bottom = 4.u),
                contentAlignment = Alignment.CenterStart,
            ) {
                TabIcon(tab, tab == current, 36, Modifier.fillMaxHeight().width(64.u), onSelect)
            }
        }
    }
}

/** Dolny pasek zakładek (pion). */
@Composable
fun TabBar(current: Screen, onSelect: (Screen) -> Unit) {
    Row(
        Modifier.fillMaxWidth().height(80.u).background(C.bg).padding(horizontal = 12.u, vertical = 8.u),
        horizontalArrangement = Arrangement.SpaceEvenly,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        for (tab in Screen.tabs) {
            TabIcon(tab, tab == current, 40, Modifier.weight(1f).padding(horizontal = 4.u).height(60.u), onSelect)
        }
    }
}
