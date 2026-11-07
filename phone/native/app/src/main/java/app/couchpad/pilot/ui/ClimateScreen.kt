package app.couchpad.pilot.ui

import androidx.compose.foundation.Canvas
import androidx.compose.foundation.gestures.detectDragGestures
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
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Add
import androidx.compose.material.icons.rounded.ArrowBackIosNew
import androidx.compose.material.icons.rounded.PowerSettingsNew
import androidx.compose.material.icons.rounded.Remove
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.input.pointer.pointerInput
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.data.ClimateState
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import java.util.Locale
import kotlin.math.PI
import kotlin.math.atan2
import kotlin.math.cos
import kotlin.math.roundToInt
import kotlin.math.sin

/** Nazwy z firmware'u (screen_climate.cpp) — identyfikatory HA → polskie podpisy. */
private val ModeNames = mapOf(
    "off" to "Wyłączona", "heat_cool" to "Auto", "auto" to "Auto", "cool" to "Chłodzenie",
    "heat" to "Grzanie", "dry" to "Osuszanie", "fan_only" to "Wentylator",
)
private val ModeCaptions = mapOf(
    "off" to "WYŁĄCZONA", "heat_cool" to "AUTO DO", "auto" to "AUTO DO", "cool" to "CHŁODZENIE DO",
    "heat" to "GRZANIE DO", "dry" to "OSUSZANIE DO", "fan_only" to "SAM NAWIEW",
)
private val FanNames = mapOf(
    "auto" to "Auto", "quiet" to "Cicha", "low" to "Niska", "medlow" to "Średnio niska",
    "medium" to "Średnia", "middle" to "Średnia", "medhigh" to "Średnio wysoka", "high" to "Wysoka",
    "strong" to "Maksymalna", "top" to "Maksymalna", "on" to "Włączona", "off" to "Wyłączona",
)
private val SwingNames = mapOf(
    "off" to "Stałe", "on" to "Ruch", "Full" to "Ruch", "both" to "Oba kierunki", "vertical" to "Pion",
    "horizontal" to "Poziom", "Upper" to "Góra", "Lower" to "Dół", "Left" to "Lewo", "Center" to "Środek",
    "Right" to "Prawo",
)

private fun degrees(v: Double) = String.format(Locale("pl"), "%.1f", v)

@Composable
fun ClimateScreen(state: PilotState, repo: PilotRepository, back: () -> Unit) {
    val c = state.home.climate
    val pending = state.pending
    var dialog by remember { mutableStateOf<String?>(null) }
    val on = c.hvacMode != "off"

    val header: @Composable () -> Unit = {
        Row(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 10.u), verticalAlignment = Alignment.CenterVertically) {
            RoundButton(Icons.Rounded.ArrowBackIosNew, size = 44, onClick = back)
            Spacer(Modifier.width(14.u))
            T("Klimatyzacja", 20, FontWeight.Bold, modifier = Modifier.weight(1f))
            RoundButton(
                Icons.Rounded.PowerSettingsNew, size = 44,
                tint = if (on) C.accentInk else C.accent,
                background = if (on) C.accent else C.accent.copy(alpha = 0.14f),
                border = if ("climate.power" in pending || on) C.accent else C.line,
            ) { repo.climatePower(!on) }
        }
    }
    val dial: @Composable (Modifier) -> Unit = { modifier ->
        TemperatureDial(c, "climate.target" in pending, modifier) { repo.climateTarget(it) }
    }
    val modes: @Composable (Modifier) -> Unit = { modifier ->
        val list = c.hvacModes.filter { it != "off" }
        if (list.isNotEmpty()) {
            val labels = list.map { ModeNames[it] ?: it }
            Segmented(labels, list.indexOf(c.hvacMode)) { i -> repo.climateMode(list[i]) }
        }
    }
    val airflow: @Composable (Modifier) -> Unit = { modifier ->
        Row(modifier, horizontalArrangement = Arrangement.spacedBy(10.u)) {
            if (c.fanModes.isNotEmpty()) {
                OptionCard("MOC", FanNames[c.fanMode] ?: c.fanMode, "climate.fan" in pending, Modifier.weight(1f)) { dialog = "fan" }
            }
            if (c.swingModes.isNotEmpty()) {
                OptionCard("PION", SwingNames[c.swingMode] ?: c.swingMode, "climate.swing" in pending, Modifier.weight(1f)) { dialog = "swing" }
            }
            if (c.swingHorizontalModes.isNotEmpty()) {
                OptionCard("POZIOM", SwingNames[c.swingHorizontalMode] ?: c.swingHorizontalMode, "climate.swing_h" in pending, Modifier.weight(1f)) {
                    dialog = "swing_h"
                }
            }
        }
    }

    if (!c.available) {
        Column(Modifier.fillMaxSize()) {
            header()
            Box(Modifier.fillMaxSize().padding(32.u), contentAlignment = Alignment.Center) {
                T("Brak świeżego stanu klimatyzacji.", 15, color = C.dim)
            }
        }
        return
    }

    if (LocalLandscape.current) {
        // Nagłówek na całą szerokość, zasilanie na końcu.
        Column(Modifier.fillMaxSize()) {
        header()
        Row(Modifier.fillMaxSize()) {
            Column(Modifier.weight(0.45f).fillMaxHeight()) {
                Box(Modifier.fillMaxSize().padding(horizontal = 16.u, vertical = 8.u), contentAlignment = Alignment.Center) {
                    dial(Modifier.fillMaxHeight())
                }
            }
            Column(
                Modifier.weight(0.55f).fillMaxHeight().padding(end = 24.u, top = 12.u, bottom = 24.u),
                verticalArrangement = Arrangement.spacedBy(10.u),
            ) {
                T("TRYB", 11, FontWeight.Medium, C.faint)
                modes(Modifier.fillMaxWidth())
                Spacer(Modifier.height(8.u))
                T("NAWIEW", 11, FontWeight.Medium, C.faint)
                airflow(Modifier.fillMaxWidth().height(80.u))
            }
        }
        }
    } else {
        Column(Modifier.fillMaxSize()) {
            header()
            Box(Modifier.fillMaxWidth().weight(1f).padding(horizontal = 56.u), contentAlignment = Alignment.Center) {
                dial(Modifier.fillMaxWidth())
            }
            Column(Modifier.fillMaxWidth().padding(horizontal = 24.u, vertical = 16.u), verticalArrangement = Arrangement.spacedBy(12.u)) {
                modes(Modifier.fillMaxWidth())
                airflow(Modifier.fillMaxWidth().height(88.u))
            }
        }
    }

    when (dialog) {
        "fan" -> OptionDialog("Moc nawiewu", c.fanModes.map { it to (FanNames[it] ?: it) }, c.fanMode, { dialog = null }, repo::climateFan)
        "swing" -> OptionDialog("Żaluzje — pion", c.swingModes.map { it to (SwingNames[it] ?: it) }, c.swingMode, { dialog = null }, repo::climateSwing)
        "swing_h" -> OptionDialog(
            "Żaluzje — poziom", c.swingHorizontalModes.map { it to (SwingNames[it] ?: it) }, c.swingHorizontalMode,
            { dialog = null }, repo::climateSwingHorizontal,
        )
    }
}

@Composable
private fun OptionCard(caption: String, value: String, pending: Boolean, modifier: Modifier, onClick: () -> Unit) {
    Card(modifier.fillMaxHeight(), radius = 18, selected = pending, onClick = onClick) {
        Column(Modifier.fillMaxSize().padding(horizontal = 16.u), verticalArrangement = Arrangement.Center) {
            T(caption, 11, FontWeight.Medium, C.faint)
            Spacer(Modifier.height(6.u))
            T(value.ifEmpty { "—" }, 15, FontWeight.Bold)
        }
    }
}

/**
 * Tarcza temperatury: łuk 270° od minimum do maksimum encji, krok z encji. Przeciąganie
 * pokazuje wartość od razu, a polecenie idzie po puszczeniu palca. W trybie „sam nawiew”
 * temperatura nie działa, więc tarcza jest wygaszona.
 */
@Composable
private fun TemperatureDial(c: ClimateState, pending: Boolean, modifier: Modifier, onSet: (Double) -> Unit) {
    var drag by remember { mutableStateOf<Double?>(null) }
    val active = c.hvacMode != "off" && c.hvacMode != "fan_only"
    val shown = drag ?: c.target
    val span = (c.max - c.min).coerceAtLeast(0.1)
    fun snap(v: Double) = (((v - c.min) / c.step).roundToInt() * c.step + c.min).coerceIn(c.min, c.max)

    BoxWithConstraints(modifier.aspectRatio(1f), contentAlignment = Alignment.Center) {
        val sizePx = constraints.maxWidth.toFloat()
        val stroke = sizePx * 0.07f
        fun valueAt(p: Offset): Double? {
            val dx = p.x - sizePx / 2
            val dy = p.y - sizePx / 2
            var fromStart = (atan2(dy, dx) * 180 / PI).toFloat() - 135f
            while (fromStart < 0) fromStart += 360f
            if (fromStart > 270f) return if (fromStart < 315f) c.max else c.min
            return snap(c.min + span * fromStart / 270f)
        }
        Canvas(
            Modifier.fillMaxSize().pointerInput(c.min, c.max, c.step, active) {
                if (!active) return@pointerInput
                detectDragGestures(
                    onDragStart = { drag = valueAt(it) },
                    onDragEnd = { drag?.let(onSet); drag = null },
                    onDragCancel = { drag = null },
                ) { change, _ -> valueAt(change.position)?.let { if (it != drag) { drag = it; haptic() } } }
            },
        ) {
            val inset = stroke / 2 + sizePx * 0.03f
            val arc = Size(size.width - 2 * inset, size.height - 2 * inset)
            val topLeft = Offset(inset, inset)
            drawArc(C.surface3, 135f, 270f, false, topLeft, arc, style = Stroke(stroke, cap = StrokeCap.Round))
            if (active && shown != null) {
                val sweep = (270f * ((shown - c.min) / span)).toFloat().coerceIn(0f, 270f)
                drawArc(
                    if (pending && drag == null) C.accent.copy(alpha = 0.55f) else C.accent,
                    135f, sweep, false, topLeft, arc, style = Stroke(stroke, cap = StrokeCap.Round),
                )
                val angle = (135f + sweep) * PI.toFloat() / 180f
                val r = arc.width / 2
                drawCircle(Color.White, stroke * 0.62f, Offset(size.width / 2 + r * cos(angle), size.height / 2 + r * sin(angle)))
            }
        }
        Column(horizontalAlignment = Alignment.CenterHorizontally) {
            T(ModeCaptions[c.hvacMode] ?: c.hvacMode.uppercase(), 12, FontWeight.Medium, C.faint)
            Row(verticalAlignment = Alignment.Top) {
                T(shown?.let { degrees(it) } ?: "—", 64, FontWeight.Bold, if (active) C.text else C.faint)
                T("°C", 22, FontWeight.Bold, if (active) C.text else C.faint, Modifier.padding(top = 8.u))
            }
            c.current?.let { T("${degrees(it)}° w pokoju", 13, color = C.dim) }
            Spacer(Modifier.height(14.u))
            Row(horizontalArrangement = Arrangement.spacedBy(20.u)) {
                RoundButton(Icons.Rounded.Remove, size = 56, tint = if (active) C.text else C.faint) {
                    if (active) c.target?.let { onSet(snap(it - c.step)) }
                }
                RoundButton(Icons.Rounded.Add, size = 56, tint = if (active) C.text else C.faint) {
                    if (active) c.target?.let { onSet(snap(it + c.step)) }
                }
            }
        }
        Row(Modifier.align(Alignment.BottomCenter).fillMaxWidth().padding(start = maxWidth * 0.1f, end = maxWidth * 0.1f, bottom = maxHeight * 0.08f)) {
            T("${degrees(c.min)}°", 11, color = C.faint)
            Spacer(Modifier.weight(1f))
            T("${degrees(c.max)}°", 11, color = C.faint)
        }
    }
}
