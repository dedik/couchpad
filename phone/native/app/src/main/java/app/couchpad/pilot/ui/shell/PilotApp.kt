package app.couchpad.pilot.ui.shell

import androidx.activity.compose.BackHandler
import androidx.compose.animation.AnimatedContent
import androidx.compose.animation.fadeIn
import androidx.compose.animation.fadeOut
import androidx.compose.animation.togetherWith
import androidx.compose.foundation.background
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.displayCutoutPadding
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.navigationBarsPadding
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.layout.systemBarsPadding
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.input.pointer.PointerEventPass
import androidx.compose.ui.input.pointer.pointerInput
import androidx.lifecycle.compose.collectAsStateWithLifecycle
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.data.zoneId
import app.couchpad.pilot.ui.appletv.AppleTvScreen
import app.couchpad.pilot.ui.audio.AudioScreen
import app.couchpad.pilot.ui.components.EdgeDrawer
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.home.HomeScreen
import app.couchpad.pilot.ui.hourIn
import app.couchpad.pilot.ui.settings.SettingsScreen
import app.couchpad.pilot.ui.start.StartScreen
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.DarkPalette
import app.couchpad.pilot.ui.theme.LightPalette
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.ProvideUnits
import app.couchpad.pilot.ui.tv.TvScreen
import kotlinx.coroutines.delay
import kotlinx.coroutines.flow.StateFlow

/** Czas na świeży stan po wybudzeniu, zanim pilot pokaże błąd (ADR 0005). */
private const val FRESH_TIMEOUT_MS = 4000L

/**
 * Cały pilot: ekrany, zakładki (na dole w pionie, w bocznym pasku w poziomie), ekran
 * łączenia, styl i wygaszanie. [homeSignal] rośnie przy Home i przy każdym zapaleniu
 * ekranu — pilot wraca wtedy na Start (ADR 0010).
 */
@Composable
fun PilotApp(repo: PilotRepository, display: DisplayControl, homeSignal: StateFlow<Int>) {
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

    fun wakeUp() {
        asleep = false
        lastTouch = System.currentTimeMillis()
        screen = Screen.Start
    }

    LaunchedEffect(screen) {
        if (screen != Screen.Home) immersive = false
        tabsDrawer = false
    }
    LaunchedEffect(Unit) {
        while (true) {
            now = System.currentTimeMillis()
            repo.expirePending()
            delay(250)
        }
    }
    LaunchedEffect(state.link.freshSince) {
        if (!state.link.fresh) wokeAt = System.currentTimeMillis()
    }

    // Styl, wygaszanie i jasność (ScreenPolicy).
    val hour = hourIn(prefs.zoneId, now)
    C.palette = if (ScreenPolicy.dark(prefs, hour)) DarkPalette else LightPalette
    val idle = now - lastTouch
    if (!asleep && ScreenPolicy.shouldSleep(prefs, idle)) asleep = true
    val dimmed = !asleep && ScreenPolicy.dimmed(prefs, idle)
    val level = ScreenPolicy.brightness(prefs, hour, asleep, dimmed)
    LaunchedEffect(level) { display.brightness(level) }
    // Uśpiony pilot oddaje ekran systemowi, który wygasi go po swoim czasie.
    LaunchedEffect(asleep) { display.keepScreenOn(!asleep) }
    LaunchedEffect(prefs.orientation) { display.orientation(prefs.orientation) }

    // Wstecz cofa do ekranu startowego; na nim niczego nie zamyka — pilot jest ekranem głównym.
    BackHandler { if (screen != Screen.Start) screen = Screen.Start }
    val home by homeSignal.collectAsStateWithLifecycle()
    // Zapalenie ekranu albo Home budzi też wygaszacz pilota — inaczej po zapaleniu
    // telefonu zostawała czarna zasłona z jasnością 1%.
    LaunchedEffect(home) { if (home > 0) wakeUp() }

    ProvideUnits {
        val content: @Composable (Modifier) -> Unit = { modifier ->
            Box(modifier) {
                AnimatedContent(
                    targetState = screen,
                    transitionSpec = { fadeIn() togetherWith fadeOut() },
                    label = "screen",
                ) { target ->
                    Box(Modifier.fillMaxSize()) {
                        ScreenContent(target, state, prefs, repo, now, onOpen = { screen = it }, onImmersive = { immersive = it })
                        if (target.needsFreshState) {
                            WakeGate(state.link, now - wokeAt > FRESH_TIMEOUT_MS, repo::wake) { screen = Screen.Settings }
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
            if (LocalLandscape.current) {
                // Poziom: zakładki w bocznym pasku (ADR 0013).
                Box(Modifier.fillMaxSize().systemBarsPadding()) {
                    Row(Modifier.fillMaxSize()) {
                        val rail = screen != Screen.Start && !immersive
                        // Pasek zakładek sam omija kamerę; ekran bez paska — cały pas przy niej.
                        if (rail) SideRail(screen) { screen = it }
                        content(Modifier.weight(1f).fillMaxHeight().then(if (rail) Modifier else Modifier.displayCutoutPadding()))
                    }
                    if (immersive) {
                        EdgeDrawer(left = true, open = tabsDrawer, onOpen = { tabsDrawer = it }, width = 88, edgeInset = cutoutWidth()) {
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
                // Uśpiony pilot: czarny ekran zabiera pierwsze dotknięcie i budzi pilota na
                // ekranie startowym (ADR 0010), a stan pobiera się w tle (ADR 0005).
                Box(
                    Modifier.fillMaxSize().background(Color.Black).tap(feedback = false) {
                        wakeUp()
                        repo.wake()
                    },
                )
            }
        }
    }
}

@Composable
private fun ScreenContent(
    screen: Screen,
    state: PilotState,
    prefs: Preferences,
    repo: PilotRepository,
    now: Long,
    onOpen: (Screen) -> Unit,
    onImmersive: (Boolean) -> Unit,
) {
    when (screen) {
        Screen.Start -> StartScreen(state, prefs, now, onRetry = repo::wake, open = onOpen)
        Screen.Tv -> TvScreen(state, prefs, repo)
        Screen.AppleTv -> AppleTvScreen(state, prefs, repo)
        Screen.Audio -> AudioScreen(state, prefs, repo)
        Screen.Home -> HomeScreen(state, prefs, repo, onImmersive)
        Screen.Settings -> SettingsScreen(state, prefs, repo)
    }
}
