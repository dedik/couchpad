package app.couchpad.pilot.ui.shell

import app.couchpad.pilot.R

/** Ekrany najwyższego poziomu; ikona to pixel-art zakładki. */
enum class Screen(val icon: Int) {
    Start(R.drawable.px_home),
    Tv(R.drawable.px_tv),
    AppleTv(R.drawable.px_appletv),
    Audio(R.drawable.px_audio),
    Home(R.drawable.px_home),
    Settings(R.drawable.px_settings);

    /** Ekran sterowania urządzeniem: pokazuje się dopiero po świeżym stanie (ADR 0005). */
    val needsFreshState get() = this in setOf(Tv, AppleTv, Audio, Home)

    companion object {
        /** Zakładki w kolejności z płytki; Start nie ma zakładki. */
        val tabs = listOf(Tv, AppleTv, Audio, Home, Settings)
    }
}

/** Sterowanie ekranem telefonu — odpowiednik `board` na płytce. */
interface DisplayControl {
    fun keepScreenOn(on: Boolean)
    fun brightness(percent: Int)
    fun orientation(index: Int)
}
