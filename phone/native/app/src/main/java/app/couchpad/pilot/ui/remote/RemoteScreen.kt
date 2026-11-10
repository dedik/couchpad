package app.couchpad.pilot.ui.remote

import androidx.compose.foundation.background
import androidx.compose.foundation.horizontalScroll
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.ColumnScope
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Option
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.data.TvState
import app.couchpad.pilot.ui.appletv.HomeKey
import app.couchpad.pilot.ui.appletv.TextDialog
import app.couchpad.pilot.ui.appletv.Trackpad
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.components.Choice
import app.couchpad.pilot.ui.components.OptionDialog
import app.couchpad.pilot.ui.components.PowerButton
import app.couchpad.pilot.ui.components.PowerStyle
import app.couchpad.pilot.ui.components.RepeatKey
import app.couchpad.pilot.ui.components.StatusDot
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.LocalLandscape
import app.couchpad.pilot.ui.theme.Logo
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.haptic
import app.couchpad.pilot.ui.theme.u
import app.couchpad.pilot.ui.tv.Shortcuts

/** Wysokość klawisza pilota (ikona pixel-art nad podpisem). */
private const val KEY_HEIGHT = 56

/** Do tylu wejść segmenty dzielą szerokość po równo; więcej — pasek przewijany. */
private const val MAX_EVEN_SOURCES = 4

private enum class RemoteDialog { Picture, Channels }

/**
 * Pilot (tylko aplikacja, makieta docs/mockups/remote-final.html): najczęstsze sterowanie
 * telewizorem, Apple TV i głośnością na jednym ekranie. Bez nagłówka ekranu — zakładkę
 * wskazuje pasek. Te same polecenia co na ekranach TV, Apple TV i Głośniki, bez nowych tematów.
 */
@Composable
fun RemoteScreen(state: PilotState, prefs: Preferences, repo: PilotRepository) {
    val tv = state.tv
    val atv = state.home.appleTv
    val z = state.z906
    var dialog by remember { mutableStateOf<RemoteDialog?>(null) }
    var keyboard by remember { mutableStateOf(false) }
    // Wejście wybrane, a jeszcze niepotwierdzone przez telewizor (obrys oczekujący).
    var requestedSource by remember { mutableStateOf<String?>(null) }
    // Jak na ekranie Apple TV: klawiatura wysuwa się sama, gdy Apple TV pokaże pole tekstowe.
    LaunchedEffect(atv.keyboardActive) { if (atv.keyboardActive && prefs.keyboardAutoOpen) keyboard = true }
    val landscape = LocalLandscape.current

    val tvStatus = if (!tv.power) "wyłączony" else listOf(tv.sourceLabel, tv.pictureLabel).filter { it.isNotEmpty() }
        .joinToString(" · ").ifEmpty { "włączony" }
    val sourcePending = state.isPending(Pending.Tv.source)
    val tvBlock: @Composable ColumnScope.() -> Unit = {
        BlockHeader(R.drawable.px_tv, "Telewizor", tvStatus, tv.power)
        Sources(tv, if (sourcePending) requestedSource else null) { id ->
            requestedSource = id
            repo.tvSource(id)
        }
        PictureStepper(tv, state.isPending(Pending.Tv.picture), onOpen = { dialog = RemoteDialog.Picture }, onSelect = repo::tvPictureMode)
        if (landscape) {
            Channels(Modifier.fillMaxWidth(), onChannel = { repo.tvChannel(it) }, onMore = { dialog = RemoteDialog.Channels })
            Spacer(Modifier.weight(1f))
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.u)) {
            Key("Cofnij", Modifier.weight(1f), R.drawable.px_key_back) { repo.tvKey("back") }
            StepKey("P −", Modifier.weight(1f).height(KEY_HEIGHT.u)) { repo.tvChannelStep(false) }
            StepKey("P +", Modifier.weight(1f).height(KEY_HEIGHT.u)) { repo.tvChannelStep(true) }
        }
    }
    val atvBlock: @Composable ColumnScope.() -> Unit = {
        BlockHeader(R.drawable.px_appletv, "Apple TV", if (atv.power) atv.appLabel ?: "włączony" else "wyłączony", atv.power) {
            PowerButton(
                atv.power, size = 38, pending = state.isPending(Pending.AppleTv.power), style = PowerStyle.Solid,
                onToggle = { repo.appleTvPower(it) },
            )
        }
        Box(Modifier.fillMaxWidth().weight(1f), contentAlignment = Alignment.Center) {
            Trackpad(Modifier.fillMaxHeight(), prefs.trackpadStep) { key, action -> repo.appleTvNavigate(key, action) }
        }
        Row(horizontalArrangement = Arrangement.spacedBy(8.u)) {
            Key("Menu", Modifier.weight(1f), R.drawable.px_key_menu) { repo.appleTvNavigate("menu") }
            HomeKey(Modifier.weight(1f), height = KEY_HEIGHT, pixel = R.drawable.px_key_home) { action -> repo.appleTvNavigate("home", action) }
            Key("Klawiatura", Modifier.weight(1f), R.drawable.px_key_keyboard) { keyboard = true }
        }
    }
    val volume = z.volume?.toString() ?: "—"
    val mute: @Composable (Modifier) -> Unit = { m ->
        Card(m, radius = 16, pending = state.isPending(Pending.Z906.mute), onClick = { repo.z906Mute(!z.muted) }) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) {
                Pixel(if (z.muted) R.drawable.px_key_mute else R.drawable.px_key_volume, 28.u)
            }
        }
    }
    val audioOn = z.power == true

    if (landscape) {
        // Poziom: Telewizor | Apple TV (najszerszy) | głośność w wąskiej kolumnie.
        Row(
            Modifier.fillMaxSize().padding(start = 12.u, end = 24.u, top = 14.u, bottom = 14.u),
            horizontalArrangement = Arrangement.spacedBy(12.u),
        ) {
            Block(Modifier.weight(0.34f).fillMaxHeight(), content = tvBlock)
            Block(Modifier.weight(0.48f).fillMaxHeight(), content = atvBlock)
            Block(Modifier.weight(0.18f).fillMaxHeight()) {
                Column(Modifier.fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally) {
                    Pixel(R.drawable.px_audio, 32.u)
                    Spacer(Modifier.height(2.u))
                    StatusDot(z.source.ifEmpty { "—" }, if (audioOn) C.ok else C.faint, size = 11)
                }
                StepKey("+", Modifier.fillMaxWidth().weight(1f)) { repo.z906VolumeStep(1) }
                T(volume, 34, FontWeight.Bold, modifier = Modifier.align(Alignment.CenterHorizontally))
                StepKey("−", Modifier.fillMaxWidth().weight(1f)) { repo.z906VolumeStep(-1) }
                mute(Modifier.fillMaxWidth().height(52.u))
            }
        }
    } else {
        Column(
            Modifier.fillMaxSize().padding(start = 16.u, end = 16.u, top = 16.u, bottom = 12.u),
            verticalArrangement = Arrangement.spacedBy(12.u),
        ) {
            Block(Modifier.fillMaxWidth(), fill = false, content = tvBlock)
            Block(Modifier.fillMaxWidth().weight(1f), content = atvBlock)
            Block(Modifier.fillMaxWidth(), fill = false) {
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Pixel(R.drawable.px_audio, 28.u)
                    Spacer(Modifier.width(8.u))
                    StatusDot(z.source.ifEmpty { "—" }, if (audioOn) C.ok else C.faint, size = 11)
                }
                Row(verticalAlignment = Alignment.CenterVertically, horizontalArrangement = Arrangement.spacedBy(10.u)) {
                    StepKey("−", Modifier.width(96.u).height(60.u)) { repo.z906VolumeStep(-1) }
                    Box(Modifier.weight(1f), contentAlignment = Alignment.Center) { T(volume, 34, FontWeight.Bold) }
                    StepKey("+", Modifier.width(96.u).height(60.u)) { repo.z906VolumeStep(1) }
                    mute(Modifier.width(60.u).height(60.u))
                }
            }
        }
    }

    val close = { dialog = null }
    when (dialog) {
        RemoteDialog.Picture -> OptionDialog(
            "Tryb obrazu", tv.pictureModes.map { Choice(it.id, it.label) }, tv.pictureMode, close, onSelect = repo::tvPictureMode,
        )
        RemoteDialog.Channels -> OptionDialog(
            "Kanały", state.channels.map { Choice(it.id, "${it.number}  ${it.name}") }, null, close,
        ) { id -> state.channels.firstOrNull { it.id == id }?.let { repo.tvChannel(it.name, it.id) } }
        null -> Unit
    }
    if (keyboard) TextDialog(prefs.keyboardLive, onDismiss = { keyboard = false }) { repo.appleTvText(it) }
}

/** Blok pilota: karta z nagłówkiem urządzenia i klawiszami. */
@Composable
private fun Block(modifier: Modifier, fill: Boolean = true, content: @Composable ColumnScope.() -> Unit) {
    Card(modifier, radius = 20) {
        // Bez [fill] blok ma wysokość treści (TV i głośność w pionie), z nim — wysokość nadaną z zewnątrz.
        Column((if (fill) Modifier.fillMaxSize() else Modifier.fillMaxWidth()).padding(12.u), verticalArrangement = Arrangement.spacedBy(10.u), content = content)
    }
}

/** Nagłówek bloku: ikona pixel-art, nazwa, stan (źródło albo aplikacja) i treść po prawej. */
@Composable
private fun BlockHeader(icon: Int, title: String, status: String, on: Boolean, trailing: @Composable RowScope.() -> Unit = {}) {
    Row(Modifier.fillMaxWidth().height(40.u), verticalAlignment = Alignment.CenterVertically) {
        Pixel(icon, 32.u)
        Spacer(Modifier.width(10.u))
        Column(Modifier.weight(1f)) {
            T(title, 15, FontWeight.Medium)
            StatusDot(status, if (on) C.ok else C.faint, size = 11)
        }
        trailing()
    }
}

/** Ikona wejścia: telewizja, Apple TV albo monitor dla pozostałych HDMI. */
private fun sourceIcon(source: Option): Int = when {
    source.id == "livetv" -> R.drawable.px_tv
    source.label.contains("apple", ignoreCase = true) -> R.drawable.px_appletv
    else -> R.drawable.px_key_home
}

/** Nazwa wejścia w segmencie: telewizja skrótem, żeby trzy wejścia zmieściły się w rzędzie. */
private fun sourceLabel(source: Option): String = if (source.id == "livetv") "TV" else source.label

/** Domyślna nazwa wejścia z telewizora („HDMI 2”) — użytkownik go nie nazwał, więc zwykle jest nieużywane. */
private val GenericInput = Regex("""HDMI\s*\d+""", RegexOption.IGNORE_CASE)

/**
 * Wejścia na Pilocie: telewizja i wejścia nazwane w ustawieniach TV (Apple TV, PC). Wejścia
 * z domyślną nazwą zostają na pełnym ekranie TV. Bieżące jest zawsze pokazane i zawsze pierwsze.
 */
internal fun remoteSources(tv: TvState): List<Option> =
    tv.sources.filter { it.id == "livetv" || it.id == tv.source || !GenericInput.matches(it.label.trim()) }
        .sortedBy { if (it.id == tv.source) 0 else 1 }

/** Wejścia telewizora jako segmenty: bieżące w akcencie, wybrane czeka na potwierdzenie. */
@Composable
private fun Sources(tv: TvState, requested: String?, onSelect: (String) -> Unit) {
    val sources = remoteSources(tv)
    if (sources.isEmpty()) return
    val even = sources.size <= MAX_EVEN_SOURCES
    Row(
        Modifier.fillMaxWidth().then(if (even) Modifier else Modifier.horizontalScroll(rememberScrollState())),
        horizontalArrangement = Arrangement.spacedBy(6.u),
    ) {
        for (source in sources) {
            Card(
                if (even) Modifier.weight(1f).height(44.u) else Modifier.width(110.u).height(44.u),
                radius = 12,
                selected = source.id == tv.source,
                pending = source.id == requested,
                background = if (source.id == tv.source) C.accentSoft else null,
                onClick = { if (source.id != tv.source) onSelect(source.id) },
            ) {
                Row(
                    Modifier.fillMaxSize().padding(horizontal = 4.u),
                    verticalAlignment = Alignment.CenterVertically,
                    horizontalArrangement = Arrangement.Center,
                ) {
                    Pixel(sourceIcon(source), 20.u)
                    Spacer(Modifier.width(4.u))
                    T(sourceLabel(source), 12, FontWeight.Medium, if (source.id == tv.source) C.text else C.dim)
                }
            }
        }
    }
}

/** Tryb obrazu jedną linią: ‹ › poprzedni / następny, nazwa otwiera listę, kropki — pozycja. */
@Composable
private fun PictureStepper(tv: TvState, pending: Boolean, onOpen: () -> Unit, onSelect: (String) -> Unit) {
    val modes = tv.pictureModes
    val index = modes.indexOfFirst { it.id == tv.pictureMode }
    fun step(delta: Int) {
        if (modes.isEmpty()) return
        val from = if (index < 0) 0 else index
        haptic()
        onSelect(modes[(from + delta + modes.size) % modes.size].id)
    }
    Card(Modifier.fillMaxWidth().height(50.u), radius = 12, pending = pending) {
        Row(Modifier.fillMaxSize(), verticalAlignment = Alignment.CenterVertically) {
            Box(Modifier.width(52.u).fillMaxHeight().tap { step(-1) }, contentAlignment = Alignment.Center) {
                T("‹", 26, color = C.dim)
            }
            Column(
                Modifier.weight(1f).fillMaxHeight().tap(onClick = onOpen),
                horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center,
            ) {
                T(tv.pictureLabel.ifEmpty { "—" }, 15, FontWeight.Bold)
                if (modes.size > 1) {
                    Spacer(Modifier.height(4.u))
                    Row(horizontalArrangement = Arrangement.spacedBy(4.u)) {
                        modes.forEachIndexed { i, _ ->
                            Box(Modifier.size(5.u).clip(CircleShape).background(if (i == index) C.accent else C.surface3))
                        }
                    }
                }
            }
            Box(Modifier.width(52.u).fillMaxHeight().tap { step(1) }, contentAlignment = Alignment.Center) {
                T("›", 26, color = C.dim)
            }
        }
    }
}

/** Skróty kanałów (jak na ekranie TV) w dwóch rzędach po trzy; ostatni — pełna lista. */
@Composable
private fun Channels(modifier: Modifier, onChannel: (String) -> Unit, onMore: () -> Unit) {
    val tile: @Composable (Modifier, @Composable () -> Unit, () -> Unit) -> Unit = { m, content, onClick ->
        Card(m.height(44.u), radius = 12, background = C.logoPlate, onClick = onClick) {
            Box(Modifier.fillMaxSize(), contentAlignment = Alignment.Center) { content() }
        }
    }
    Column(modifier, verticalArrangement = Arrangement.spacedBy(6.u)) {
        for (row in listOf(Shortcuts.take(3), Shortcuts.drop(3))) {
            Row(horizontalArrangement = Arrangement.spacedBy(6.u)) {
                for ((name, logo) in row) tile(Modifier.weight(1f), { Logo(logo, 30.u) }) { onChannel(name) }
                if (row.size < 3) {
                    tile(Modifier.weight(1f), { Pixel(R.drawable.px_fx_more, 26.u) }, onMore)
                    repeat(2 - row.size) { Spacer(Modifier.weight(1f)) }
                }
            }
        }
    }
}

/** Klawisz pilota: ikona pixel-art nad podpisem. */
@Composable
private fun Key(label: String, modifier: Modifier, icon: Int, onClick: () -> Unit) {
    Card(modifier.height(KEY_HEIGHT.u), radius = 16, onClick = onClick) {
        Column(Modifier.fillMaxSize(), horizontalAlignment = Alignment.CenterHorizontally, verticalArrangement = Arrangement.Center) {
            Pixel(icon, 24.u)
            Spacer(Modifier.height(4.u))
            T(label, 12, FontWeight.Medium, C.dim)
        }
    }
}

/** Klawisz kroku (P±, głośność ±): przytrzymanie powtarza, jak kołyski na ekranie TV. */
@Composable
private fun StepKey(label: String, modifier: Modifier, onStep: () -> Unit) {
    Card(modifier, radius = 16) { RepeatKey(label, Modifier.fillMaxSize(), onStep) }
}
