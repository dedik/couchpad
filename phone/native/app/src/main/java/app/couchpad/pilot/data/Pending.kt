package app.couchpad.pilot.data

/**
 * Klucze poleceń czekających na potwierdzenie stanem (`PilotState.pending`). Pierwszy człon
 * to urządzenie: nowy stan z jego tematów kasuje wszystkie klucze z tym prefiksem, a
 * oczekiwanie starsze niż limit wygasa i UI pokazuje brak potwierdzenia.
 */
object Pending {
    object Z906 {
        const val prefix = "z906."
        const val power = "z906.power"
        const val mute = "z906.mute"
        const val volume = "z906.volume"
        const val source = "z906.source"
        const val effect = "z906.effect"
        fun level(channel: Z906Channel) = "z906.${channel.key}"
    }

    object Tv {
        const val prefix = "tv."
        const val source = "tv.source"
        const val picture = "tv.picture"
    }

    object AppleTv {
        const val prefix = "appletv."
        const val power = "appletv.power"
        const val app = "appletv.app"
    }

    object Climate {
        const val prefix = "climate."
        const val power = "climate.power"
        const val target = "climate.target"
        const val mode = "climate.mode"
        const val fan = "climate.fan"
        const val swing = "climate.swing"
        const val swingHorizontal = "climate.swing_h"
    }

    object Light {
        const val prefix = "light."
        fun of(id: String) = "light.$id"
    }

    /** Grzejnik (ADR 0018): osobno zasilanie, tryb i nastawa każdej głowicy. */
    object Radiator {
        const val prefix = "radiator."
        fun power(id: String) = "radiator.$id.power"
        fun mode(id: String) = "radiator.$id.mode"
        fun target(id: String) = "radiator.$id.target"
        fun any(id: String) = "radiator.$id."
    }
}

/** Oczekiwania bez kluczy zaczynających się od któregoś z prefiksów. */
internal fun Map<String, Long>.withoutPrefixes(vararg prefixes: String): Map<String, Long> =
    filterKeys { key -> prefixes.none { key.startsWith(it) } }
