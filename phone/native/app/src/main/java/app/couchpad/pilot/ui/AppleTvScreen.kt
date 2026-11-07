package app.couchpad.pilot.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.gestures.awaitEachGesture
import androidx.compose.foundation.gestures.awaitFirstDown
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.BoxWithConstraints
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.aspectRatio
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.Home
import androidx.compose.material.icons.rounded.Keyboard
import androidx.compose.material.icons.rounded.KeyboardArrowDown
import androidx.compose.material.icons.rounded.KeyboardArrowLeft
import androidx.compose.material.icons.rounded.KeyboardArrowRight
import androidx.compose.material.icons.rounded.KeyboardArrowUp
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.input.pointer.positionChange
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import androidx.compose.ui.window.Dialog
import app.couchpad.pilot.R
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import kotlinx.coroutines.delay
import kotlin.math.abs
import kotlin.math.hypot

private val AppLogos = mapOf(
    "com.firecore.infuse" to R.drawable.logo_infuse,
    "com.google.ios.youtube" to R.drawable.logo_youtube,
    "pl.pilotwp.appletv" to R.drawable.logo_wppilot,
)

@Composable
fun AppleTvScreen(state: PilotState, repo: PilotRepository) {
    val atv = state.home.appleTv
    var keyboard by remember { mutableStateOf(false) }
    // Klawiatura wysuwa się sama, gdy Apple TV pokaże pole tekstowe.
    val prefs = repo.prefs.value
    LaunchedEffect(atv.keyboardActive) { if (atv.keyboardActive && prefs.keyboardAutoOpen) keyboard = true }

    val header: @Composable () -> Unit = {
        Header(
            R.drawable.px_appletv, "Apple TV", repo.prefs.value.roomName,
            power = atv.power, powerPending = "appletv.power" in state.pending,
            onPower = { repo.appleTvPower(it) },
        )
    }
    val apps: @Composable (Modifier, Arrangement.Horizontal) -> Unit = { modifier, arrangement ->
        Row(modifier.height(68.u), horizontalArrangement = arrangement) {
            for (app in atv.apps.take(6)) {
                val active = app.id == atv.appId
                Card(Modifier.size(68.u), radius = 18, selected = active, onClick = { repo.appleTvLaunch(app.id) }) {
                    Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                        val logo = AppLogos[app.id]
                        if (logo != null) Logo(logo, 40.u) else T(app.label, 11, FontWeight.Medium, maxLines = 2)
                    }
                }
            }
        }
    }
    val menu: @Composable (Modifier) -> Unit = { m -> Button("Menu", m, icon = Icons.Rounded.ArrowBackIosNew) { repo.appleTvNavigate("menu") } }
    val home: @Composable (Modifier) -> Unit = { m -> HomeKey(m) { action -> repo.appleTvNavigate("home", action) } }
    val text: @Composable (Modifier) -> Unit = { m -> Button("Tekst", m, icon = Icons.Rounded.Keyboard) { keyboard = true } }

    if (LocalLandscape.current) {
        // Nagłówek na całą szerokość, zasilanie na końcu.
        Column(Modifier.fillMaxSize()) {
        header()
        Row(Modifier.fillMaxSize()) {
            Column(Modifier.weight(0.45f).fillMaxHeight()) {
                apps(Modifier.fillMaxWidth().padding(horizontal = 24.u), Arrangement.spacedBy(12.u))
                Spacer(Modifier.weight(1f))
                Column(Modifier.fillMaxWidth().padding(24.u), verticalArrangement = Arrangement.spacedBy(12.u)) {
                    menu(Modifier.fillMaxWidth())
                    home(Modifier.fillMaxWidth())
                    text(Modifier.fillMaxWidth())
                }
            }
            Box(Modifier.weight(0.55f).fillMaxHeight().padding(16.u), contentAlignment = Alignment.Center) {
                Trackpad(Modifier.fillMaxHeight(), prefs.trackpadStep) { key, action -> repo.appleTvNavigate(key, action) }
            }
        }
        }
    } else {
        Column(Modifier.fillMaxSize()) {
            header()
            apps(Modifier.fillMaxWidth().padding(horizontal = 24.u), Arrangement.spacedBy(12.u, Alignment.CenterHorizontally))
            Box(Modifier.fillMaxWidth().weight(1f).padding(horizontal = 24.u, vertical = 16.u), contentAlignment = Alignment.Center) {
                Trackpad(Modifier.fillMaxWidth(), prefs.trackpadStep) { key, action -> repo.appleTvNavigate(key, action) }
            }
            Row(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 12.u), horizontalArrangement = Arrangement.spacedBy(12.u)) {
                menu(Modifier.weight(1f))
                home(Modifier.weight(1f))
                text(Modifier.weight(1f))
            }
        }
    }
    if (keyboard) TextDialog(prefs.keyboardLive, onDismiss = { keyboard = false }) { repo.appleTvText(it) }
}

/**
 * Gładzik jak pilot Siri: stuknięcie strzałki lub środka to pojedynczy klawisz,
 * przesunięcie palcem przewija listy krokami, przytrzymanie środka otwiera menu kontekstowe.
 */
@Composable
private fun Trackpad(modifier: Modifier, stepUnits: Int, onKey: (String, String) -> Unit) {
    var lit by remember { mutableStateOf<String?>(null) }
    BoxWithConstraints(modifier.aspectRatio(1f)) {
        val sizePx = constraints.maxWidth.toFloat()
        // Krok gładzika w pikselach makiety (Klawiatura i gładzik → Czułość).
        val stepPx = stepUnits * LocalUnits.current.dpPerUnit * androidx.compose.ui.platform.LocalDensity.current.density
        Box(
            Modifier.fillMaxSize().clip(CircleShape).background(C.surface).border(1.u, C.line, CircleShape)
                .pointerInput(Unit) {
                    awaitEachGesture {
                        val down = awaitFirstDown()
                        val start = down.position
                        var acc = Offset.Zero
                        var moved = false
                        val downAt = System.currentTimeMillis()
                        var holdSent = false
                        while (true) {
                            val event = awaitPointerEvent()
                            val change = event.changes.firstOrNull() ?: break
                            if (!change.pressed) break
                            acc += change.positionChange()
                            if (abs(acc.x) > stepPx || abs(acc.y) > stepPx) {
                                moved = true
                                val key = if (abs(acc.x) > abs(acc.y)) (if (acc.x > 0) "right" else "left") else (if (acc.y > 0) "down" else "up")
                                lit = key
                                haptic()
                                onKey(key, "tap")
                                acc = Offset.Zero
                            }
                            if (!moved && !holdSent && System.currentTimeMillis() - downAt > 600 &&
                                hypot(start.x - sizePx / 2, start.y - sizePx / 2) < sizePx * 0.22f
                            ) {
                                holdSent = true
                                haptic()
                                onKey("select", "hold")
                            }
                        }
                        if (!moved && !holdSent) {
                            val dx = start.x - sizePx / 2
                            val dy = start.y - sizePx / 2
                            val key = when {
                                hypot(dx, dy) < sizePx * 0.22f -> "select"
                                abs(dx) > abs(dy) -> if (dx > 0) "right" else "left"
                                else -> if (dy > 0) "down" else "up"
                            }
                            lit = key
                            haptic()
                            onKey(key, "tap")
                        }
                        lit = null
                    }
                },
            contentAlignment = Alignment.Center,
        ) {
            val tint = { k: String -> if (lit == k) C.accent else C.faint }
            Icon(Icons.Rounded.KeyboardArrowUp, null, tint = tint("up"), modifier = Modifier.align(Alignment.TopCenter).padding(top = 20.u).size(28.u))
            Icon(Icons.Rounded.KeyboardArrowDown, null, tint = tint("down"), modifier = Modifier.align(Alignment.BottomCenter).padding(bottom = 20.u).size(28.u))
            Icon(Icons.Rounded.KeyboardArrowLeft, null, tint = tint("left"), modifier = Modifier.align(Alignment.CenterStart).padding(start = 20.u).size(28.u))
            Icon(Icons.Rounded.KeyboardArrowRight, null, tint = tint("right"), modifier = Modifier.align(Alignment.CenterEnd).padding(end = 20.u).size(28.u))
            Box(
                Modifier.fillMaxSize(0.4f).clip(CircleShape).background(if (lit == "select") C.surface3 else C.surface2)
                    .border(1.u, C.line, CircleShape),
                contentAlignment = Alignment.Center,
            ) { T("OK", 14, FontWeight.Medium, C.faint) }
        }
    }
}

/** Home: stuknięcie, podwójne (przełącznik aplikacji) i przytrzymanie (Centrum sterowania). */
@Composable
private fun HomeKey(modifier: Modifier, onAction: (String) -> Unit) {
    var lastTap by remember { mutableStateOf(0L) }
    var pendingTap by remember { mutableStateOf(0L) }
    LaunchedEffect(pendingTap) {
        if (pendingTap != 0L) {
            delay(300)
            onAction("tap")
            pendingTap = 0L
        }
    }
    Box(
        modifier.height(56.u).clip(RoundedCornerShape(16.u)).background(C.surface).border(1.u, C.line, RoundedCornerShape(16.u))
            .pointerInput(Unit) {
                awaitEachGesture {
                    awaitFirstDown()
                    haptic()
                    val downAt = System.currentTimeMillis()
                    var held = false
                    while (true) {
                        val event = awaitPointerEvent()
                        if (event.changes.none { it.pressed }) break
                        if (!held && System.currentTimeMillis() - downAt > 400) {
                            held = true
                            onAction("hold")
                        }
                    }
                    if (!held) {
                        val now = System.currentTimeMillis()
                        if (pendingTap != 0L && now - lastTap < 300) {
                            pendingTap = 0L
                            onAction("double")
                        } else {
                            pendingTap = now
                        }
                        lastTap = now
                    }
                }
            },
        contentAlignment = Alignment.Center,
    ) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Icon(Icons.Rounded.Home, null, tint = C.text, modifier = Modifier.size(20.u))
            Spacer(Modifier.size(8.u))
            T("Home", 15, FontWeight.Medium)
        }
    }
}

/** Tekst na Apple TV: klawiatura telefonu, wysyłka na żywo (appletv.keyboard.set). */
@Composable
private fun TextDialog(live: Boolean, onDismiss: () -> Unit, onText: (String) -> Unit) {
    var text by remember { mutableStateOf("") }
    val focus = remember { FocusRequester() }
    // Na żywo: tekst idzie na Apple TV w trakcie pisania; inaczej dopiero po „Gotowe”.
    LaunchedEffect(text) {
        if (!live) return@LaunchedEffect
        delay(250)
        onText(text)
    }
    LaunchedEffect(Unit) { focus.requestFocus() }
    Dialog(onDismissRequest = onDismiss) {
        ProvideUnits {
            Column(
                Modifier.fillMaxWidth().clip(RoundedCornerShape(24.u)).background(C.surface)
                    .border(1.u, C.line, RoundedCornerShape(24.u)).padding(20.u),
            ) {
                T("Tekst na Apple TV", 19, FontWeight.Bold)
                Spacer(Modifier.height(14.u))
                BasicTextField(
                    value = text,
                    onValueChange = { text = it },
                    singleLine = true,
                    textStyle = style(18),
                    cursorBrush = SolidColor(C.accent),
                    keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
                    keyboardActions = KeyboardActions(onDone = { onText(text); onDismiss() }),
                    modifier = Modifier.fillMaxWidth().focusRequester(focus).clip(RoundedCornerShape(12.u))
                        .background(C.surface2).border(1.u, C.line, RoundedCornerShape(12.u))
                        .padding(horizontal = 14.u, vertical = 14.u),
                )
                Spacer(Modifier.height(14.u))
                Button("Gotowe", Modifier.fillMaxWidth(), accent = true) { onText(text); onDismiss() }
            }
        }
    }
}
