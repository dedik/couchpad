package app.couchpad.pilot

import android.app.Activity
import android.content.pm.ActivityInfo
import android.view.WindowManager
import app.couchpad.pilot.ui.shell.DisplayControl

/** To, czym na płytce steruje `board`: podświetlenie, wygaszanie i orientacja — tu przez okno. */
class WindowDisplay(private val activity: Activity) : DisplayControl {
    private val window get() = activity.window

    override fun keepScreenOn(on: Boolean) {
        if (on) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
    }

    override fun brightness(percent: Int) {
        val attrs = window.attributes
        val value = percent.coerceIn(1, 100) / 100f
        if (attrs.screenBrightness != value) {
            attrs.screenBrightness = value
            window.attributes = attrs
        }
    }

    /** Indeks z Ustawienia → Ekran → Orientacja: auto, pion, poziom, poziom odwrócony. */
    override fun orientation(index: Int) {
        activity.requestedOrientation = when (index) {
            1 -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
            2 -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            3 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
            else -> ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
        }
    }
}
