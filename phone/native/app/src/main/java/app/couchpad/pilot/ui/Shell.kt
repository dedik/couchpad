package app.couchpad.pilot.ui

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.ui.layout.positionInWindow
import androidx.compose.ui.layout.onGloballyPositioned
import androidx.compose.ui.unit.Dp
import androidx.compose.ui.unit.LayoutDirection
import androidx.compose.ui.platform.LocalDensity
import androidx.compose.ui.platform.LocalView
import androidx.compose.foundation.layout.displayCutout
import androidx.compose.foundation.layout.WindowInsets
import androidx.compose.foundation.clickable
import androidx.compose.foundation.interaction.MutableInteractionSource
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.displayCutoutPadding
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.PowerSettingsNew
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.WarningAmber
import androidx.compose.material3.CircularProgressIndicator
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.material3.Text
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import app.couchpad.pilot.data.Options
import app.couchpad.pilot.R
import app.couchpad.pilot.data.LinkPhase
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import kotlinx.coroutines.delay

enum class Screen(val icon: Int) {
    Start(R.drawable.px_home),
    Tv(R.drawable.px_tv),
    AppleTv(R.drawable.px_appletv),
    Audio(R.drawable.px_audio),
    Home(R.drawable.px_home),
    Settings(R.drawable.px_settings),
}

private val Tabs = listOf(Screen.Tv, Screen.AppleTv, Screen.Audio, Screen.Home, Screen.Settings)

/** Czas na świeży stan po wybudzeniu, zanim pilot pokaże błąd (ADR 0005). */
private const val FreshTimeoutMs = 4000L

/** Sterowanie ekranem telefonu — odpowiednik `board` na płytce. */
interface DisplayControl {
    fun keepScreenOn(on: Boolean)
    fun brightness(percent: Int)
    fun orientation(index: Int)
}

/** Przyciemnienie przed wygaszeniem (ui.cpp: kDimPercent). */
private const val DimPercent = 15
private const val DimBeforeMs = 10_000L

@Composable
fun PilotApp(repo: PilotRepository, display: DisplayControl) {
    val state by repo.state.collectAsStateWithLifecycle()
    val prefs by repo.prefs.collectAsStateWithLifecycle()
    var screen by remember { mutableStateOf(Screen.Start) }
    var now by remember { mutableLongStateOf(System.currentTimeMillis()) }
    var wokeAt by remember { mutableLongStateOf(System.currentTimeMillis()) }
    var lastTouch by remember { mutableLongStateOf(System.currentTimeMillis()) }
    var asleep by remember { mutableStateOf(false) }
    // Dom w głębi (pokój, lampa…) w poziomie: zakładki chowają się do lewej szuflady.
    var immersive by remember { mutableStateOf(false) }
    var tabsDrawer by remember { mutableStateOf(false) }
    LaunchedEffect(screen) { if (screen != Screen.Home) immersive = false; tabsDrawer = false }

    LaunchedEffect(Unit) {
        while (true) {
            now = System.currentTimeMillis()
            repo.expirePending()
            delay(250)
        }
    }
    LaunchedEffect(state.link.freshSince) {
        if (state.link.freshSince == 0L) wokeAt = System.currentTimeMillis()
    }

    // Styl: ciemny, jasny albo wg godzin (ADR 0012).
    val zone = java.util.TimeZone.getTimeZone(Options.timeZones.getOrElse(prefs.timeZone) { "Europe/Warsaw" })
    val hour = java.util.Calendar.getInstance(zone).apply { timeInMillis = now }.get(java.util.Calendar.HOUR_OF_DAY)
    val dark = when (prefs.theme) {
        1 -> false
        2 -> Options.hourIn(Options.themeHours.getOrElse(prefs.themeHours) { 18 to 8 }, hour)
        else -> true
    }
    C.palette = if (dark) DarkPalette else LightPalette

    // Wygaszanie po bezczynności i przyciemnianie 10 s przed nim.
    val timeout = Options.timeouts.getOrElse(prefs.timeout) { 30_000L }
    val idle = now - lastTouch
    if (!asleep && timeout > 0 && idle > timeout) asleep = true
    val dimmed = !asleep && timeout > 0 && prefs.dimBeforeSleep && idle > timeout - DimBeforeMs
    val night = Options.nightWindows.getOrElse(prefs.night) { 0 to 0 }
    val chosen = if (Options.hourIn(night, hour)) minOf(prefs.brightness, Options.nightBrightness) else prefs.brightness
    val level = when {
        asleep -> 1
        dimmed -> minOf(chosen, DimPercent)
        else -> chosen
    }
    LaunchedEffect(level) { display.brightness(level) }
    // Uśpiony pilot oddaje ekran systemowi, który wygasi go po swoim czasie.
    LaunchedEffect(asleep) { display.keepScreenOn(!asleep) }
    LaunchedEffect(prefs.orientation) { display.orientation(prefs.orientation) }

    // Wstecz cofa do ekranu startowego; na nim niczego nie zamyka — pilot jest ekranem głównym.
    BackHandler { if (screen != Screen.Start) screen = Screen.Start }
    val homePressed by app.couchpad.pilot.Graph.homePressed.collectAsStateWithLifecycle()
    LaunchedEffect(homePressed) { if (homePressed > 0) screen = Screen.Start }

    ProvideUnits {
        val landscape = LocalLandscape.current
        val content: @Composable (Modifier) -> Unit = { modifier ->
            Box(modifier) {
                AnimatedContent(
                    targetState = screen,
                    transitionSpec = { fadeIn() togetherWith fadeOut() },
                    label = "screen",
                ) { target ->
                    Box(Modifier.fillMaxSize()) {
                        when (target) {
                            Screen.Start -> StartScreen(state, repo, now) { screen = it }
                            Screen.Tv -> TvScreen(state, repo)
                            Screen.AppleTv -> AppleTvScreen(state, repo)
                            Screen.Audio -> AudioScreen(state, repo)
                            Screen.Home -> HomeScreen(state, repo) { immersive = it }
                            Screen.Settings -> SettingsScreen(state, repo)
                        }
                        if (target in DeviceScreens) {
                            WakeGate(state, now - wokeAt > FreshTimeoutMs, repo) { screen = Screen.Settings }
                        }
                    }
                }
            }
        }
        Box(
            Modifier.fillMaxSize().background(C.bg).pointerInput(Unit) {
                // Każdy dotyk odnawia czas do wygaszenia; nie zabiera zdarzenia kontrolkom.
                awaitPointerEventScope {
                    while (true) {
                        awaitPointerEvent(PointerEventPass.Initial)
                        lastTouch = System.currentTimeMillis()
                    }
                }
            },
        ) {
            if (landscape) {
                // Poziom: zakładki w bocznym pasku (ADR 0013).
                Box(Modifier.fillMaxSize().systemBarsPadding()) {
                    Row(Modifier.fillMaxSize()) {
                        val rail = screen != Screen.Start && !immersive
                        // Pasek zakładek sam omija kamerę; ekran bez paska — cały pas przy niej.
                        if (rail) SideRail(screen) { screen = it }
                        content(Modifier.weight(1f).fillMaxHeight().then(if (rail) Modifier else Modifier.displayCutoutPadding()))
                    }
                    if (immersive) {
                        EdgeDrawer(
                            left = true, open = tabsDrawer, onOpen = { tabsDrawer = it }, width = 88,
                            edgeInset = cutoutWidth(),
                        ) {
                            SideRail(screen) { tabsDrawer = false; screen = it }
                        }
                    }
                }
            } else {
                Column(Modifier.fillMaxSize().statusBarsPadding().displayCutoutPadding().navigationBarsPadding()) {
                    content(Modifier.weight(1f).fillMaxWidth())
                    if (screen != Screen.Start) TabBar(screen) { screen = it }
                }
            }
            if (asleep) {
                // Uśpiony pilot: czarny ekran zabiera pierwsze dotknięcie i budzi pilota,
                // który pobiera świeży stan (ADR 0005).
                Box(
                    Modifier.fillMaxSize().background(Color.Black)
                        .clickable(remember { MutableInteractionSource() }, null) {
                            asleep = false
                            lastTouch = System.currentTimeMillis()
                            // Po wybudzeniu ekran startowy (ADR 0010), stan pobierany w tle.
                            screen = Screen.Start
                            repo.wake()
                        },
                )
            }
        }
    }
}

/** Wycięcie na kamerę przy lewej krawędzi (poziom): prostokąt w pikselach okna albo null. */
@Composable
private fun leftCutout(): android.graphics.Rect? {
    val view = LocalView.current
    val left = WindowInsets.displayCutout.getLeft(LocalDensity.current, LayoutDirection.Ltr)
    return remember(left, view) {
        if (left == 0 || android.os.Build.VERSION.SDK_INT < 28) null
        else view.rootWindowInsets?.displayCutout?.boundingRects?.firstOrNull { it.left <= 0 && it.width() > 0 }
    }
}

@Composable
fun cutoutWidth(): Dp = with(LocalDensity.current) { (leftCutout()?.right ?: 0).toDp() }

/**
 * Boczny pasek zakładek. Ikony stoją przy samej krawędzi wyświetlacza, także w pasie obok
 * przedniej kamery; tylko ikona, która wypada na wysokości kamery, jest odsunięta o szerokość
 * wycięcia.
 */
@Composable
private fun SideRail(current: Screen, onSelect: (Screen) -> Unit) {
    val cut = leftCutout()
    val cutWidth = cutoutWidth()
    Column(
        Modifier.fillMaxHeight().width(80.u + cutWidth).background(C.bg).padding(vertical = 10.u),
        verticalArrangement = Arrangement.SpaceEvenly,
    ) {
        for (tab in Tabs) {
            val selected = tab == current
            var top by remember { mutableStateOf(0f) }
            var bottom by remember { mutableStateOf(0f) }
            val atCamera = cut != null && bottom > cut.top && top < cut.bottom
            Box(
                Modifier
                    .weight(1f)
                    .fillMaxWidth()
                    .onGloballyPositioned { c ->
                        val y = c.positionInWindow().y
                        top = y
                        bottom = y + c.size.height
                    }
                    .padding(start = if (atCamera) cutWidth + 8.u else 8.u, top = 4.u, bottom = 4.u),
                contentAlignment = Alignment.CenterStart,
            ) {
                Box(
                    Modifier
                        .fillMaxHeight()
                        .width(64.u)
                        .clip(RoundedCornerShape(16.u))
                        .background(if (selected) C.accent.copy(alpha = 0.16f) else Color.Transparent)
                        .clickable(remember { MutableInteractionSource() }, null) {
                            haptic()
                            onSelect(tab)
                        },
                    contentAlignment = Alignment.Center,
                ) {
                    Pixel(tab.icon, 36.u)
                }
            }
        }
    }
}

private val DeviceScreens = setOf(Screen.Tv, Screen.AppleTv, Screen.Audio, Screen.Home)

/**
 * Ekran łączenia zakrywa sterowanie, dopóki po wybudzeniu nie przyjdzie świeży stan.
 * Nie pokazujemy zapamiętanego stanu jako aktualnego (ADR 0005).
 */
@Composable
private fun WakeGate(state: PilotState, timedOut: Boolean, repo: PilotRepository, openSettings: () -> Unit) {
    if (state.link.freshSince != 0L) return
    val failed = state.link.phase == LinkPhase.Error || timedOut
    Column(
        Modifier
            .fillMaxSize()
            .background(C.bg)
            .clickable(remember { MutableInteractionSource() }, null) {}
            .padding(horizontal = 48.u),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.Center,
    ) {
        Box(
            Modifier.size(88.u).clip(CircleShape)
                .background(if (failed) C.danger.copy(alpha = 0.14f) else C.accent.copy(alpha = 0.12f)),
            contentAlignment = Alignment.Center,
        ) {
            if (failed) {
                Icon(Icons.Rounded.WarningAmber, null, tint = C.danger, modifier = Modifier.size(36.u))
            } else {
                CircularProgressIndicator(color = C.accent, strokeWidth = 3.u, modifier = Modifier.size(40.u))
            }
        }
        Spacer(Modifier.height(32.u))
        T(if (failed) "Brak połączenia z domem" else "Łączenie z domem", 26, FontWeight.Bold)
        Spacer(Modifier.height(10.u))
        Text(
            when {
                !failed -> "Pobieram aktualny stan urządzeń"
                state.link.error != null -> state.link.error + ". Nie pokazuję zapamiętanego stanu."
                else -> "Dom nie odpowiedział. Nie pokazuję zapamiętanego stanu."
            },
            style = style(14, color = C.dim),
            textAlign = TextAlign.Center,
        )
        Spacer(Modifier.height(20.u))
        Row(horizontalArrangement = Arrangement.spacedBy(16.u)) {
            val mqtt = state.link.phase == LinkPhase.Connected
            StatusDot("MQTT", if (mqtt) C.ok else if (failed) C.danger else C.faint)
            StatusDot("Synchronizacja", if (failed && mqtt) C.danger else C.faint)
        }
        if (failed) {
            Spacer(Modifier.height(28.u))
            Button("Ponów", Modifier.width(240.u), icon = Icons.Rounded.Refresh, accent = true) { repo.wake() }
            Spacer(Modifier.height(12.u))
            Button("Ustawienia", Modifier.width(240.u), icon = Icons.Rounded.Settings, onClick = openSettings)
        }
    }
}

@Composable
private fun TabBar(current: Screen, onSelect: (Screen) -> Unit) {
    Row(
        Modifier
            .fillMaxWidth()
            .height(80.u)
            .background(C.bg)
            .padding(horizontal = 12.u, vertical = 8.u),
        horizontalArrangement = Arrangement.SpaceEvenly,
        verticalAlignment = Alignment.CenterVertically,
    ) {
        for (tab in Tabs) {
            val selected = tab == current
            Box(
                Modifier
                    .weight(1f)
                    .padding(horizontal = 4.u)
                    .height(60.u)
                    .clip(RoundedCornerShape(16.u))
                    .background(if (selected) C.accent.copy(alpha = 0.16f) else Color.Transparent)
                    .clickable(remember { MutableInteractionSource() }, null) {
                        haptic()
                        onSelect(tab)
                    },
                contentAlignment = Alignment.Center,
            ) {
                Pixel(tab.icon, 40.u)
            }
        }
    }
}

/** Nagłówek ekranu urządzenia: ikona, nazwa, pokój i przycisk zasilania. */
@Composable
fun Header(
    icon: Int,
    title: String,
    subtitle: String,
    power: Boolean? = null,
    powerPending: Boolean = false,
    extra: @Composable () -> Unit = {},
    onPower: ((Boolean) -> Unit)? = null,
) {
    Row(
        Modifier.fillMaxWidth().padding(start = 24.u, end = 24.u, top = 10.u, bottom = 6.u).height(56.u),
        verticalAlignment = Alignment.CenterVertically,
    ) {
        Box(Modifier.size(40.u).clip(RoundedCornerShape(12.u)).background(C.surface2), contentAlignment = Alignment.Center) {
            Pixel(icon, 30.u)
        }
        Spacer(Modifier.width(12.u))
        Column(Modifier.weight(1f)) {
            T(title, 19, FontWeight.Bold)
            T(subtitle.uppercase(), 11, FontWeight.Medium, C.faint)
        }
        extra()
        if (onPower != null) {
            Spacer(Modifier.width(10.u))
            val on = power == true
            RoundButton(
                Icons.Rounded.PowerSettingsNew,
                size = 44,
                tint = if (on) C.accentInk else C.dim,
                background = if (on) C.accent else C.surface2,
                border = if (powerPending) C.accent else if (on) C.accent else C.line,
            ) { onPower(!on) }
        }
    }
}
