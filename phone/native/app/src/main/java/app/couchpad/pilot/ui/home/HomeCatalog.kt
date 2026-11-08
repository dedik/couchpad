package app.couchpad.pilot.ui.home

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.AcUnit
import androidx.compose.material.icons.rounded.Alarm
import androidx.compose.material.icons.rounded.AutoAwesome
import androidx.compose.material.icons.rounded.Bedtime
import androidx.compose.material.icons.rounded.Block
import androidx.compose.material.icons.rounded.Celebration
import androidx.compose.material.icons.rounded.CenterFocusStrong
import androidx.compose.material.icons.rounded.LightMode
import androidx.compose.material.icons.rounded.LocalFireDepartment
import androidx.compose.material.icons.rounded.MonitorHeart
import androidx.compose.material.icons.rounded.Nightlight
import androidx.compose.material.icons.rounded.Settings
import androidx.compose.material.icons.rounded.Spa
import androidx.compose.material.icons.rounded.Tv
import androidx.compose.material.icons.rounded.WbIncandescent
import androidx.compose.material.icons.rounded.WbSunny
import androidx.compose.material.icons.rounded.WbTwilight
import androidx.compose.material.icons.rounded.Weekend
import androidx.compose.ui.graphics.vector.ImageVector
import app.couchpad.pilot.R

/** Pomieszczenie zakładki Dom (screen_home.cpp): identyfikator z bridge'u, nazwa, ikona. */
internal data class Room(val id: String, val label: String, val icon: Int)

internal val Rooms = listOf(
    Room("salon", "Salon", R.drawable.px_room_salon),
    Room("kuchnia", "Kuchnia", R.drawable.px_room_kuchnia),
    Room("przedpokoj", "Przedpokój", R.drawable.px_room_przedpokoj),
    Room("biuro", "Biuro", R.drawable.px_room_biuro),
    Room("lazienka", "Łazienka", R.drawable.px_room_lazienka),
    Room("sypialnia", "Sypialnia", R.drawable.px_room_sypialnia),
)

internal fun room(id: String) = Rooms.first { it.id == id }

/** Klimatyzacja z kontraktu (ADR 0009) wisi w salonie. */
internal const val ClimateRoom = "salon"

/** Efekty HA → polska nazwa i ikona (odpowiedniki ikon Lucide z firmware'u). */
internal object Effects {
    private data class Effect(val label: String, val icon: ImageVector)

    private val known = mapOf(
        "Cozy" to Effect("Przytulnie", Icons.Rounded.Weekend),
        "Relax" to Effect("Relaks", Icons.Rounded.Spa),
        "TV time" to Effect("Seans TV", Icons.Rounded.Tv),
        "Candlelight" to Effect("Świeca", Icons.Rounded.LocalFireDepartment),
        "Warm white" to Effect("Ciepła biel", Icons.Rounded.WbIncandescent),
        "Golden white" to Effect("Złota biel", Icons.Rounded.WbSunny),
        "Daylight" to Effect("Dzienne", Icons.Rounded.LightMode),
        "Cool white" to Effect("Zimna biel", Icons.Rounded.AcUnit),
        "Focus" to Effect("Skupienie", Icons.Rounded.CenterFocusStrong),
        "Night light" to Effect("Nocne", Icons.Rounded.Nightlight),
        "Bedtime" to Effect("Do snu", Icons.Rounded.Bedtime),
        "Wake-up" to Effect("Pobudka", Icons.Rounded.WbTwilight),
        "Pulse" to Effect("Puls", Icons.Rounded.MonitorHeart),
        "Steampunk" to Effect("Steampunk", Icons.Rounded.Settings),
        "Diwali" to Effect("Diwali", Icons.Rounded.Celebration),
        "Alarm" to Effect("Alarm", Icons.Rounded.Alarm),
    )

    fun name(id: String) = if (id.isEmpty()) "Bez efektu" else known[id]?.label ?: id
    fun icon(id: String) = if (id.isEmpty()) Icons.Rounded.Block else known[id]?.icon ?: Icons.Rounded.AutoAwesome
}

/** „1 ŻARÓWKA”, „3 ŻARÓWKI”, „5 ŻARÓWEK”. */
internal fun bulbs(n: Int) = when {
    n == 1 -> "1 ŻARÓWKA"
    n % 10 in 2..4 && n % 100 !in 12..14 -> "$n ŻARÓWKI"
    else -> "$n ŻARÓWEK"
}

/** Obrazek lampki: niedostępna, zgaszona, przygaszona (< 50%) albo jasna. */
internal fun lampImage(available: Boolean, on: Boolean, brightness: Int) = when {
    !available -> R.drawable.px_lamp_na
    !on -> R.drawable.px_lamp_off
    brightness >= 50 -> R.drawable.px_lamp_on
    else -> R.drawable.px_lamp_dim
}
