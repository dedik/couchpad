package app.couchpad.pilot.ui.start

import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.RoundedCornerShape
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.R
import app.couchpad.pilot.data.Weather
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.noDecimals
import app.couchpad.pilot.ui.oneDecimal
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.Pixel
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u

/** Pion: pogoda w prawym górnym rogu, pod nią pigułka z temperaturą w domu. */
@Composable
internal fun WeatherBlock(w: Weather) {
    Column(horizontalAlignment = Alignment.End) {
        Row(verticalAlignment = Alignment.CenterVertically) {
            Pixel(weatherIcon(w.condition, w.night), 44.u)
            Spacer(Modifier.width(10.u))
            Column(horizontalAlignment = Alignment.End) {
                w.temperature?.let { T(noDecimals(it) + "°C", 26, FontWeight.Bold) }
                T(weatherName(w.condition), 12, color = C.dim)
            }
        }
        w.indoorTemperature?.let { t ->
            Spacer(Modifier.height(10.u))
            val shape = RoundedCornerShape(12.u)
            Row(
                Modifier.clip(shape).background(C.surface).border(1.u, C.line, shape).padding(horizontal = 10.u, vertical = 6.u),
                verticalAlignment = Alignment.CenterVertically,
            ) {
                T("W DOMU", 10, FontWeight.Medium, C.faint)
                Spacer(Modifier.width(8.u))
                T(oneDecimal(t) + "°", 14, FontWeight.Bold)
                w.indoorHumidity?.let {
                    Spacer(Modifier.width(6.u))
                    T("$it%", 12, color = C.dim)
                }
            }
        }
    }
}

/** Poziom: dwa kafle pogody — na zewnątrz i w domu. */
@Composable
internal fun WeatherTiles(w: Weather) {
    Row(horizontalArrangement = Arrangement.spacedBy(10.u)) {
        WeatherTile(
            "NA ZEWNĄTRZ", weatherIcon(w.condition, w.night),
            w.temperature?.let { noDecimals(it) + "°C" } ?: "—", weatherName(w.condition), Modifier.weight(1f),
        )
        if (w.indoorTemperature != null) {
            WeatherTile(
                "W DOMU", R.drawable.px_home, oneDecimal(w.indoorTemperature) + "°",
                w.indoorHumidity?.let { "wilgotność $it%" } ?: "", Modifier.weight(1f),
            )
        }
    }
}

@Composable
private fun WeatherTile(caption: String, icon: Int, value: String, note: String, modifier: Modifier) {
    Card(modifier.height(96.u), radius = 18) {
        Column(Modifier.padding(12.u)) {
            T(caption, 10, FontWeight.Medium, C.faint)
            Spacer(Modifier.height(6.u))
            Row(verticalAlignment = Alignment.CenterVertically) {
                Pixel(icon, 32.u)
                Spacer(Modifier.width(8.u))
                T(value, 22, FontWeight.Bold)
            }
            T(note, 11, color = C.dim)
        }
    }
}

/** Ikona pixel-art dla stanu pogody z HA; w nocy księżyc zamiast słońca. */
internal fun weatherIcon(condition: String, night: Boolean): Int = when (condition) {
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

internal fun weatherName(condition: String): String = when (condition) {
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
