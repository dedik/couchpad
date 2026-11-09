package app.couchpad.pilot.ui.home

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.RowScope
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxSize
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
import androidx.compose.ui.draw.alpha
import androidx.compose.ui.draw.clip
import androidx.compose.ui.graphics.Brush
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Pending
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Radiator
import app.couchpad.pilot.ui.components.Dial
import app.couchpad.pilot.ui.components.PageHeader
import app.couchpad.pilot.ui.components.PowerButton
import app.couchpad.pilot.ui.components.tap
import app.couchpad.pilot.ui.noDecimals
import app.couchpad.pilot.ui.oneDecimal
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Caption
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u
import app.couchpad.pilot.ui.upperPl
import kotlin.math.abs

/** Grzejnik (ADR 0018): nazwy trybów głowicy i wspólne teksty karty i ekranu. */
internal object RadiatorLabels {
    private val modes = mapOf("off" to "Wyłączony", "heat" to "Grzanie", "auto" to "Harmonogram")

    fun mode(id: String) = modes[id] ?: id

    fun caption(r: Radiator) = when {
        !r.available -> "NIEDOSTĘPNY"
        !r.on -> "WYŁĄCZONY"
        r.mode == "auto" -> "HARMONOGRAM"
        else -> "GRZANIE"
    }

    /** Stan na karcie pokoju: „Wyłączony”, „Grzanie · 20,5°”, „Harmonogram · 19,0°”. */
    fun status(r: Radiator) = when {
        !r.available -> "niedostępny"
        !r.on || r.target == null -> mode(r.mode)
        else -> "${mode(r.mode)} · ${oneDecimal(r.target)}°"
    }

    /** Dopisek karty: temperatura z czujnika pokoju (nigdy z głowicy) i stan zaworu. */
    fun note(r: Radiator, room: Double?): String? {
        val parts = listOfNotNull(
            room?.let { "w pokoju ${oneDecimal(it)}°" },
            when {
                !r.available || !r.on -> null
                r.windowOpen == true -> "okno otwarte"
                r.heating -> "grzeje"
                else -> "nie grzeje"
            },
        )
        return parts.takeIf { it.isNotEmpty() }?.joinToString(" · ")
    }

    /** Różnica ze znakiem; minus jako półpauza: „+0,4°”, „–1,5°”. */
    fun delta(v: Double): String {
        val text = oneDecimal(abs(v)) + "°"
        return when {
            text == oneDecimal(0.0) + "°" -> text
            v > 0 -> "+$text"
            else -> "–$text"
        }
    }
}

/** Skala tarczy grzejnika: encja pozwala 4–35 °C, tarcza pokazuje 5–30. */
private const val DialMin = 5.0
private const val DialMax = 30.0

/** Szybkie nastawy (kafle „SZYBKO”). */
private val QuickTargets = listOf(17.0, 19.0, 21.0, 23.0)

/** Kolor „grzeje” (zawór otwarty) — ten sam w obu stylach, jak w makiecie. */
internal val HeatInk = Color(0xFFFF7E5F)

/**
 * Łuk od zimna do gorąca jako pędzel: ułamki od początku łuku (0 = start, 0,75 = koniec,
 * bo łuk ma 270°). Za końcem znowu niebieski, żeby zaokrąglony początek nie złapał czerwieni.
 */
private val HeatBrush = Brush.sweepGradient(
    0f to Color(0xFF3D8BFF),
    0.375f to Color(0xFFF2A93B),
    0.75f to Color(0xFFE5281B),
    0.87f to Color(0xFFE5281B),
    0.88f to Color(0xFF3D8BFF),
    1f to Color(0xFF3D8BFF),
)

/**
 * Grzejnik (Dom → pokój → Grzejnik, ADR 0018): tarcza nastawy z gradientem i znacznikiem
 * temperatury z czujnika pokoju, zasilanie w nagłówku, kafle Tryb / Teraz / Szybko.
 */
@Composable
internal fun RadiatorPage(
    roomId: String,
    radiator: Radiator,
    state: PilotState,
    repo: PilotRepository,
    back: () -> Unit,
    switchRoom: (String) -> Unit,
) {
    val r = radiator
    // Temperatura w pokoju wyłącznie z czujnika pokoju (wiadomość pogody), jak karta „W pokoju”.
    val roomTemp = state.weather.takeIf { it.current(System.currentTimeMillis()) }?.room(roomId)?.temperature
    val targetPending = state.isPending(Pending.Radiator.target(r.id))
    val modePending = state.isPending(Pending.Radiator.mode(r.id))
    // Ostatnio dotknięty kafel — on, nie stary wybór, pokazuje oczekiwanie.
    var touchedQuick by remember { mutableStateOf<Double?>(null) }
    var touchedMode by remember { mutableStateOf<String?>(null) }
    val enabled = r.available && r.on
    val lo = r.min.coerceAtLeast(DialMin)
    val hi = r.max.coerceAtMost(DialMax)
    val (min, max) = if (hi > lo) lo to hi else DialMin to DialMax

    DialPage(
        roomId, switchRoom, scrollSide = true, dialInset = 76,
        header = {
            PageHeader("Grzejnik", "${upperPl(room(roomId).label)} · GŁOWICA", back) {
                if (r.available) {
                    PowerButton(r.on, pending = state.isPending(Pending.Radiator.power(r.id))) { repo.radiatorPower(r.id, it) }
                }
            }
        },
        dial = { m ->
            Dial(
                value = if (enabled) r.target else null,
                min = min, max = max, step = r.step,
                caption = RadiatorLabels.caption(r),
                detail = roomTemp?.let { "w pokoju ${oneDecimal(it)}°" },
                unit = if (enabled) "°C" else "",
                format = ::oneDecimal,
                modifier = m,
                enabled = enabled,
                pending = targetPending,
                minLabel = "${noDecimals(min)}°",
                maxLabel = "${noDecimals(max)}°",
                brush = HeatBrush,
                marker = roomTemp,
                detailDot = true,
            ) { touchedQuick = null; repo.radiatorTarget(r.id, it) }
        },
    ) {
        val modes = listOf("heat" to R.drawable.px_mode_heat, "auto" to R.drawable.px_trv_auto).filter { it.first in r.modes }
        if (modes.isNotEmpty()) {
            Section("TRYB") {
                for ((id, icon) in modes) {
                    val selected = enabled && r.mode == id
                    Tile(selected, pending = modePending && touchedMode == id, Modifier.height(64.u), onClick = {
                        if (r.available) { touchedMode = id; repo.radiatorMode(r.id, id) }
                    }) {
                        Row(verticalAlignment = Alignment.CenterVertically) {
                            Pixel(icon, 24.u)
                            Spacer(Modifier.width(10.u))
                            T(RadiatorLabels.mode(id), 15, FontWeight.Medium, if (selected) C.accent else C.text)
                        }
                    }
                }
            }
        }
        Section("TERAZ") {
            val valve = when {
                !r.on -> Status(R.drawable.px_trv_valve_off, "ZAWÓR", "wyłączony")
                r.heats -> Status(R.drawable.px_trv_valve_on, "ZAWÓR", "grzeje", HeatInk)
                else -> Status(R.drawable.px_trv_valve_off, "ZAWÓR", "nie grzeje", if (r.windowOpen == true) C.accent else null)
            }
            StatusTile(valve)
            r.windowOpen?.let { open ->
                StatusTile(
                    if (open) Status(R.drawable.px_trv_window_open, "OKNO", "otwarte", C.accent)
                    else Status(R.drawable.px_trv_window_closed, "OKNO", "zamknięte"),
                )
            }
            r.battery?.let { b ->
                val low = b < 30
                StatusTile(Status(if (low) R.drawable.px_trv_battery_low else R.drawable.px_trv_battery_ok, "BATERIA", "$b%", if (low) C.accent else null))
            }
            val next = r.next
            StatusTile(
                if (r.on && r.mode == "auto" && next != null) Status(R.drawable.px_trv_next, "OD ${next.at}", "${oneDecimal(next.target)}°")
                else Status(
                    R.drawable.px_trv_delta, "CEL – POKÓJ",
                    if (enabled && r.target != null && roomTemp != null) RadiatorLabels.delta(r.target - roomTemp) else "—",
                ),
            )
        }
        Section("SZYBKO") {
            for (v in QuickTargets) {
                val selected = enabled && r.target == v
                Tile(selected, pending = targetPending && touchedQuick == v, Modifier.height(64.u), onClick = {
                    if (enabled) { touchedQuick = v; repo.radiatorTarget(r.id, v) }
                }) {
                    T("${noDecimals(v)}°", 15, FontWeight.Bold, if (selected) C.accent else if (enabled) C.text else C.faint)
                }
            }
        }
    }
}

@Composable
private fun Section(caption: String, tiles: @Composable RowScope.() -> Unit) {
    Column {
        Caption(caption)
        Spacer(Modifier.height(8.u))
        Row(Modifier.fillMaxWidth(), horizontalArrangement = Arrangement.spacedBy(6.u), content = tiles)
    }
}

/** Kafel do wyboru: wybrany na miękkim akcencie z obwódką, oczekujący z przygaszoną obwódką. */
@Composable
private fun RowScope.Tile(
    selected: Boolean,
    pending: Boolean,
    modifier: Modifier,
    onClick: () -> Unit,
    content: @Composable () -> Unit,
) {
    val shape = RoundedCornerShape(14.u)
    Box(
        modifier.weight(1f).clip(shape)
            .background(if (selected) C.accentSoft else C.surface2)
            .then(
                when {
                    pending -> Modifier.border(1.u, C.accentPending, shape)
                    selected -> Modifier.border(1.u, C.accent, shape)
                    else -> Modifier
                },
            )
            .tap(onClick = onClick),
        contentAlignment = Alignment.Center,
    ) { content() }
}

/** Odczyt kafla „TERAZ”: ikona 24 px, podpis wersalikami i wartość (kolor ostrzeżenia/grzania). */
private data class Status(val icon: Int, val caption: String, val value: String, val color: Color? = null)

@Composable
private fun RowScope.StatusTile(s: Status) {
    val shape = RoundedCornerShape(14.u)
    Column(
        Modifier.weight(1f).height(84.u).clip(shape).background(C.surface2).padding(6.u),
        horizontalAlignment = Alignment.CenterHorizontally,
        verticalArrangement = Arrangement.spacedBy(2.u, Alignment.CenterVertically),
    ) {
        Pixel(s.icon, 24.u)
        Caption(s.caption)
        T(s.value, 15, FontWeight.Medium, s.color ?: C.text)
    }
}

/** Ikona studzienki karty: grzeje / nie grzeje; wyłączony przygaszony. */
@Composable
internal fun RadiatorWell(r: Radiator) {
    Pixel(
        if (r.heats) R.drawable.px_dev_radiator else R.drawable.px_dev_radiator_idle, 48.u,
        if (r.on && r.available) Modifier else Modifier.alpha(0.45f),
    )
}
