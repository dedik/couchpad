package app.couchpad.pilot.ui.appletv

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.foundation.text.BasicTextField
import androidx.compose.foundation.text.KeyboardActions
import androidx.compose.foundation.text.KeyboardOptions
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.Keyboard
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.rememberUpdatedState
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.focus.FocusRequester
import androidx.compose.ui.focus.focusRequester
import androidx.compose.ui.graphics.SolidColor
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.input.ImeAction
import app.couchpad.pilot.R
import app.couchpad.pilot.data.AppleTvState
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.ui.components.Button
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.DeviceHeader
import app.couchpad.pilot.ui.components.DialogCard
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Logo
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.style
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.delay

private val AppLogos = mapOf(
    "com.firecore.infuse" to R.drawable.logo_infuse,
    "com.google.ios.youtube" to R.drawable.logo_youtube,
    "pl.pilotwp.appletv" to R.drawable.logo_wppilot,
)

/** Apple TV przez bridge HA (ADR 0007): aplikacje, gładzik, Menu/Home/Tekst. */
@Composable
fun AppleTvScreen(state: PilotState, prefs: Preferences, repo: PilotRepository) {
    val atv = state.home.appleTv
    var keyboard by remember { mutableStateOf(false) }
    // Klawiatura wysuwa się sama, gdy Apple TV pokaże pole tekstowe.
    LaunchedEffect(atv.keyboardActive) { if (atv.keyboardActive && prefs.keyboardAutoOpen) keyboard = true }

    val navigate: (String, String) -> Unit = { key, action -> repo.appleTvNavigate(key, action) }
    val actions: @Composable (Modifier) -> Unit = { m ->
        Button("Menu", m, icon = Icons.Rounded.ArrowBackIosNew) { repo.appleTvNavigate("menu") }
        HomeKey(m) { action -> repo.appleTvNavigate("home", action) }
        Button("Tekst", m, icon = Icons.Rounded.Keyboard) { keyboard = true }
    }

    Column(Modifier.fillMaxSize()) {
        DeviceHeader(
            R.drawable.px_appletv, "Apple TV", prefs.roomName,
            power = atv.power, powerPending = state.isPending(Pending.AppleTv.power),
            onPower = { repo.appleTvPower(it) },
        )
        if (LocalLandscape.current) {
            Row(Modifier.fillMaxSize()) {
                Column(Modifier.weight(0.45f).fillMaxHeight()) {
                    AppRow(atv, Modifier.fillMaxWidth().padding(horizontal = 24.u), Arrangement.spacedBy(12.u), repo::appleTvLaunch)
                    Spacer(Modifier.weight(1f))
                    Column(Modifier.fillMaxWidth().padding(24.u), verticalArrangement = Arrangement.spacedBy(12.u)) {
                        actions(Modifier.fillMaxWidth())
                    }
                }
                Box(Modifier.weight(0.55f).fillMaxHeight().padding(16.u), contentAlignment = Alignment.Center) {
                    Trackpad(Modifier.fillMaxHeight(), prefs.trackpadStep, navigate)
                }
            }
        } else {
            AppRow(
                atv, Modifier.fillMaxWidth().padding(horizontal = 24.u),
                Arrangement.spacedBy(12.u, Alignment.CenterHorizontally), repo::appleTvLaunch,
            )
            Box(Modifier.fillMaxWidth().weight(1f).padding(horizontal = 24.u, vertical = 16.u), contentAlignment = Alignment.Center) {
                Trackpad(Modifier.fillMaxWidth(), prefs.trackpadStep, navigate)
            }
            Row(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 12.u), horizontalArrangement = Arrangement.spacedBy(12.u)) {
                actions(Modifier.weight(1f))
            }
        }
    }
    if (keyboard) TextDialog(prefs.keyboardLive, onDismiss = { keyboard = false }) { repo.appleTvText(it) }
}

/** Do sześciu aplikacji z bridge'u: logo, a bez logo nazwa; bieżąca z obrysem. */
@Composable
private fun AppRow(atv: AppleTvState, modifier: Modifier, arrangement: Arrangement.Horizontal, onLaunch: (String) -> Unit) {
    Row(modifier.height(68.u), horizontalArrangement = arrangement) {
        for (app in atv.apps.take(6)) {
            val logo = AppLogos[app.id]
            Card(
                Modifier.size(68.u), radius = 18, selected = app.id == atv.appId,
                background = if (logo != null) C.logoPlate else null, onClick = { onLaunch(app.id) },
            ) {
                Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                    if (logo != null) Logo(logo, 40.u) else T(app.label, 11, FontWeight.Medium, maxLines = 2)
                }
            }
        }
    }
}

/** Tekst na Apple TV: klawiatura telefonu, wysyłka na żywo (appletv.keyboard.set). */
@Composable
private fun TextDialog(live: Boolean, onDismiss: () -> Unit, onText: (String) -> Unit) {
    var text by remember { mutableStateOf("") }
    val send by rememberUpdatedState(onText)
    val focus = remember { FocusRequester() }
    // Na żywo: tekst idzie na Apple TV w trakcie pisania; inaczej dopiero po „Gotowe”.
    LaunchedEffect(text) {
        if (!live) return@LaunchedEffect
        delay(250)
        send(text)
    }
    LaunchedEffect(Unit) { focus.requestFocus() }
    val done = { send(text); onDismiss() }
    DialogCard("Tekst na Apple TV", onDismiss) {
        val shape = RoundedCornerShape(12.u)
        BasicTextField(
            value = text,
            onValueChange = { text = it },
            singleLine = true,
            textStyle = style(18),
            cursorBrush = SolidColor(C.accent),
            keyboardOptions = KeyboardOptions(imeAction = ImeAction.Done),
            keyboardActions = KeyboardActions(onDone = { done() }),
            modifier = Modifier.fillMaxWidth().focusRequester(focus).clip(shape)
                .background(C.surface2).border(1.u, C.line, shape).padding(14.u),
        )
        Button("Gotowe", Modifier.fillMaxWidth(), accent = true, onClick = done)
    }
}
