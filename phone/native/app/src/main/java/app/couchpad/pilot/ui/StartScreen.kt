package app.couchpad.pilot.ui

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
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.Refresh
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.LinkPhase
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import java.text.SimpleDateFormat
import java.util.Date
import java.util.Locale

private val Polish = Locale("pl", "PL")

@Composable
fun StartScreen(state: PilotState, repo: PilotRepository, now: Long, open: (Screen) -> Unit) {
    val prefs = repo.prefs.value
    val date = Date(now)
    val fresh = state.link.freshSince != 0L
    if (LocalLandscape.current) {
        StartLandscape(state, repo, date, fresh, open)
        return
    }
    Column(Modifier.fillMaxSize().padding(horizontal = 24.u)) {
        Spacer(Modifier.height(20.u))
        Row(verticalAlignment = Alignment.Top) {
            Column(Modifier.weight(1f)) {
                T(formatIn("H:mm", prefs.timeZone, date), 72, FontWeight.Bold)
                T(
                    formatIn("EEEE, d MMMM", prefs.timeZone, date) + " · " + prefs.roomName.uppercase(),
                    13, color = C.dim,
                )
            }
            WeatherBlock(state)
        }
        Spacer(Modifier.height(24.u))
        Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(12.u)) {
            Tile(R.drawable.px_tv, "Telewizor", tvStatus(state), state.tv.available && fresh, Modifier.weight(1f)) { open(Screen.Tv) }
            Tile(R.drawable.px_appletv, "Apple TV", appleStatus(state), state.home.appleTv.available && fresh, Modifier.weight(1f)) { open(Screen.AppleTv) }
        }
        Spacer(Modifier.height(12.u))
        Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(12.u)) {
            Tile(R.drawable.px_audio, "Głośniki", audioStatus(state), state.z906.available && fresh, Modifier.weight(1f)) { open(Screen.Audio) }
            Tile(R.drawable.px_home, "Dom", homeStatus(state), state.home.known, Modifier.weight(1f)) { open(Screen.Home) }
        }
        Row(
            Modifier.fillMaxWidth().height(84.u),
            verticalAlignment = Alignment.CenterVertically,
        ) {
            val connected = state.link.phase == LinkPhase.Connected && fresh
            StatusDot(
                when {
                    connected -> "Połączono z domem"
                    state.link.phase == LinkPhase.Connecting -> "Łączenie z domem…"
                    state.link.phase == LinkPhase.Connected -> "Czekam na stan z domu…"
                    else -> state.link.error ?: "Brak połączenia z domem"
                },
                if (connected) C.ok else if (state.link.phase == LinkPhase.Error) C.danger else C.accent,
                Modifier.weight(1f),
            )
            if (state.link.phase == LinkPhase.Error) {
                Button("Ponów", Modifier.width(120.u), icon = Icons.Rounded.Refresh, accent = true, height = 48) { repo.wake() }
                Spacer(Modifier.width(10.u))
            }
            RoundButton(Icons.Rounded.Settings, size = 48) { open(Screen.Settings) }
        }
        Spacer(Modifier.height(4.u))
    }
}

/** Poziom: zegar, pogoda i stan z lewej, kafelki 2 × 2 z prawej (ADR 0013). */
@Composable
private fun StartLandscape(state: PilotState, repo: PilotRepository, date: Date, fresh: Boolean, open: (Screen) -> Unit) {
    val prefs = repo.prefs.value
    Row(Modifier.fillMaxSize().padding(20.u), horizontalArrangement = Arrangement.spacedBy(16.u)) {
        Column(Modifier.weight(0.42f).fillMaxHeight()) {
            Column(Modifier.weight(1f).fillMaxWidth(), horizontalAlignment = Alignment.CenterHorizontally,
                verticalArrangement = Arrangement.Center) {
                T(formatIn("H:mm", prefs.timeZone, date), 80, FontWeight.Bold)
                T(formatIn("EEEE, d MMMM", prefs.timeZone, date) + " · " + prefs.roomName.uppercase(), 13, color = C.dim)
            }
            WeatherTiles(state)
            Spacer(Modifier.height(14.u))
            Row(verticalAlignment = Alignment.CenterVertically) {
                val connected = state.link.phase == LinkPhase.Connected && fresh
                StatusDot(
                    if (connected) "Połączono z domem" else state.link.error ?: "Łączenie z domem…",
                    if (connected) C.ok else if (state.link.phase == LinkPhase.Error) C.danger else C.accent,
                    Modifier.weight(1f),
                )
                if (state.link.phase == LinkPhase.Error) {
                    RoundButton(Icons.Rounded.Refresh, size = 44, tint = C.accentInk, background = C.accent) { repo.wake() }
                    Spacer(Modifier.width(8.u))
                }
                RoundButton(Icons.Rounded.Settings, size = 44) { open(Screen.Settings) }
            }
        }
        Column(Modifier.weight(0.58f).fillMaxHeight(), verticalArrangement = Arrangement.spacedBy(12.u)) {
            Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(12.u)) {
                Tile(R.drawable.px_tv, "Telewizor", tvStatus(state), state.tv.available && fresh, Modifier.weight(1f), 72) { open(Screen.Tv) }
                Tile(R.drawable.px_appletv, "Apple TV", appleStatus(state), state.home.appleTv.available && fresh, Modifier.weight(1f), 72) { open(Screen.AppleTv) }
            }
            Row(Modifier.weight(1f), horizontalArrangement = Arrangement.spacedBy(12.u)) {
                Tile(R.drawable.px_audio, "Głośniki", audioStatus(state), state.z906.available && fresh, Modifier.weight(1f), 72) { open(Screen.Audio) }
                Tile(R.drawable.px_home, "Dom", homeStatus(state), state.home.known, Modifier.weight(1f), 72) { open(Screen.Home) }
            }
        }
    }
}

/** Dwa kafle pogody w poziomie: na zewnątrz i w domu. */
@Composable
private fun WeatherTiles(state: PilotState) {
    val w = state.weather
    if (w.receivedAt == 0L || System.currentTimeMillis() - w.receivedAt > 6 * 3600_000L) return
    Row(horizontalArrangement = Arrangement.spacedBy(10.u)) {
        Card(Modifier.weight(1f).height(96.u), radius = 18) {
            Column(Modifier.padding(12.u)) {
                T("NA ZEWNĄTRZ", 10, FontWeight.Medium, C.faint)
                Spacer(Modifier.height(6.u))
                Row(verticalAlignment = Alignment.CenterVertically) {
                    Pixel(weatherIcon(w.condition, w.night), 32.u)
                    Spacer(Modifier.width(8.u))
                    T(w.temperature?.let { String.format(Polish, "%.0f°C", it) } ?: "—", 22, FontWeight.Bold)
                }
                T(weatherName(w.condition), 11, color = C.dim)
            }
        }
        if (w.indoorTemperature != null) {
            Card(Modifier.weight(1f).height(96.u), radius = 18) {
                Column(Modifier.padding(12.u)) {
                    T("W DOMU", 10, FontWeight.Medium, C.faint)
                    Spacer(Modifier.height(6.u))
                    Row(verticalAlignment = Alignment.CenterVertically) {
                        Pixel(R.drawable.px_home, 32.u)
                        Spacer(Modifier.width(8.u))
                        T(String.format(Polish, "%.1f°", w.indoorTemperature), 22, FontWeight.Bold)
                    }
                    T(w.indoorHumidity?.let { "wilgotność $it%" } ?: "", 11, color = C.dim)
                }
            }
        }
    }
}

@Composable
private fun Tile(icon: Int, title: String, status: String, ok: Boolean, modifier: Modifier, iconSize: Int = 96, onClick: () -> Unit) {
    Card(modifier.fillMaxHeight(), radius = 24, onClick = onClick) {
        Column(Modifier.fillMaxSize().padding(if (iconSize < 96) 14.u else 20.u)) {
            Box(Modifier.weight(1f).fillMaxWidth(), contentAlignment = Alignment.Center) {
                Pixel(icon, iconSize.u)
            }
            T(title, if (iconSize < 96) 22 else 26, FontWeight.Bold)
            Spacer(Modifier.height(8.u))
            StatusDot(status, if (ok) C.accent else C.danger, size = 13)
        }
    }
}

@Composable
private fun WeatherBlock(state: PilotState) {
    val w = state.weather
    if (w.receivedAt == 0L || System.currentTimeMillis() - w.receivedAt > 6 * 3600_000L) return
    Column(horizontalAlignment = Alignment.End) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Pixel(weatherIcon(w.condition, w.night), 44.u)
            Spacer(Modifier.width(10.u))
            Column(horizontalAlignment = Alignment.End) {
                w.temperature?.let { T(String.format(Polish, "%.0f°C", it), 26, FontWeight.Bold) }
                T(weatherName(w.condition), 12, color = C.dim)
            }
        }
        w.indoorTemperature?.let { t ->
            Spacer(Modifier.height(10.u))
            Row(
                Modifier.clip(RoundedCornerShape(12.u)).background(C.surface).border(1.u, C.line, RoundedCornerShape(12.u))
                    .padding(horizontal = 10.u, vertical = 6.u),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                T("W DOMU", 10, FontWeight.Medium, C.faint)
                Spacer(Modifier.width(8.u))
                T(String.format(Polish, "%.1f°", t), 14, FontWeight.Bold)
                w.indoorHumidity?.let {
                    Spacer(Modifier.width(6.u))
                    T("$it%", 12, color = C.dim)
                }
            }
        }
    }
}

private fun tvStatus(s: PilotState): String {
    val tv = s.tv
    if (!tv.known || !tv.available) return "brak połączenia"
    if (!tv.power) return "wyłączony"
    return listOf(tv.sourceLabel, tv.pictureLabel).filter { it.isNotEmpty() }.joinToString(" · ").ifEmpty { "włączony" }
}

private fun appleStatus(s: PilotState): String {
    val a = s.home.appleTv
    if (!s.home.known || !a.available) return "brak połączenia"
    if (!a.power) return "uśpione"
    return a.apps.firstOrNull { it.id == a.appId }?.label ?: "ekran główny"
}

private fun audioStatus(s: PilotState): String {
    val z = s.z906
    if (!z.known || !z.available) return "brak połączenia"
    if (z.power == false) return "wyłączone"
    return listOfNotNull(z.volume?.let { "głośność $it" }, z.source.ifEmpty { null }).joinToString(" · ")
}

private fun homeStatus(s: PilotState): String {
    if (!s.home.known) return "brak połączenia"
    val on = s.home.lights.count { it.on }
    val climate = s.home.climate
    val parts = mutableListOf<String>()
    if (climate.available) parts += if (climate.hvacMode == "off") "klima wyłączona" else "klima ${climate.target?.let { String.format(Polish, "%.1f°", it) } ?: ""}"
    parts += if (on == 0) "światła zgaszone" else "światła: $on"
    return parts.joinToString(" · ")
}

/** Data i godzina w strefie z ustawień (Pokój i czas). */
fun formatIn(pattern: String, zone: Int, date: Date): String =
    SimpleDateFormat(pattern, Polish).apply {
        timeZone = java.util.TimeZone.getTimeZone(app.couchpad.pilot.data.Options.timeZones.getOrElse(zone) { "Europe/Warsaw" })
    }.format(date)

fun weatherIcon(condition: String, night: Boolean): Int = when (condition) {
    "sunny" -> if (night) R.drawable.px_weather_night else R.drawable.px_weather_sunny
    "clear-night" -> R.drawable.px_weather_night
    "partlycloudy" -> if (night) R.drawable.px_weather_partly_night else R.drawable.px_weather_partly
    "cloudy" -> R.drawable.px_weather_cloudy
    "fog" -> R.drawable.px_weather_fog
    "rainy", "pouring" -> R.drawable.px_weather_rain
    "snowy", "snowy-rainy", "hail" -> R.drawable.px_weather_snow
    "lightning", "lightning-rainy" -> R.drawable.px_weather_storm
    "windy", "windy-variant" -> R.drawable.px_weather_wind
    else -> R.drawable.px_weather_cloudy
}

fun weatherName(condition: String): String = when (condition) {
    "sunny" -> "słonecznie"
    "clear-night" -> "bezchmurnie"
    "partlycloudy" -> "częściowe zachmurzenie"
    "cloudy" -> "pochmurno"
    "fog" -> "mgła"
    "rainy" -> "deszcz"
    "pouring" -> "ulewa"
    "snowy" -> "śnieg"
    "snowy-rainy" -> "deszcz ze śniegiem"
    "hail" -> "grad"
    "lightning", "lightning-rainy" -> "burza"
    "windy", "windy-variant" -> "wietrznie"
    else -> condition
}
