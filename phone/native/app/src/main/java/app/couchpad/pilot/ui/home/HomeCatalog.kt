package app.couchpad.pilot.ui.home

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
    /** Ikona to pixel art 24 x 24 z firmware'u (tools/draw_option_icons.py, `px_fx_*`). */
    private data class Effect(val label: String, val icon: Int)

    private val known = mapOf(
        "Cozy" to Effect("Przytulnie", R.drawable.px_fx_cozy),
        "Relax" to Effect("Relaks", R.drawable.px_fx_relax),
        "TV time" to Effect("Seans TV", R.drawable.px_fx_tv),
        "Candlelight" to Effect("Świeca", R.drawable.px_fx_candle),
        "Warm white" to Effect("Ciepła biel", R.drawable.px_fx_warm),
        "Golden white" to Effect("Złota biel", R.drawable.px_fx_golden),
        "Daylight" to Effect("Dzienne", R.drawable.px_fx_daylight),
        "Cool white" to Effect("Zimna biel", R.drawable.px_fx_cool),
        "Focus" to Effect("Skupienie", R.drawable.px_fx_focus),
        "Night light" to Effect("Nocne", R.drawable.px_fx_night),
        "Bedtime" to Effect("Do snu", R.drawable.px_fx_bedtime),
        "Wake-up" to Effect("Pobudka", R.drawable.px_fx_wakeup),
        "Pulse" to Effect("Puls", R.drawable.px_fx_pulse),
        "Steampunk" to Effect("Steampunk", R.drawable.px_fx_steampunk),
        "Diwali" to Effect("Diwali", R.drawable.px_fx_diwali),
        "Alarm" to Effect("Alarm", R.drawable.px_fx_alarm),
    )

    fun name(id: String) = if (id.isEmpty()) "Bez efektu" else known[id]?.label ?: id
    fun icon(id: String) = if (id.isEmpty()) R.drawable.px_fx_none else known[id]?.icon ?: R.drawable.px_mode_auto
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
