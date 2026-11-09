package app.couchpad.pilot

import android.app.Activity
import android.content.pm.ActivityInfo
import android.os.Build
import android.view.Surface
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
        val value = percent.coerceIn(0, 100) / 100f
        if (attrs.screenBrightness != value) {
            attrs.screenBrightness = value
            window.attributes = attrs
        }
    }

    private var index = 0
    private var pinned = false

    /** Indeks z Ustawienia → Ekran → Orientacja: auto, pion, poziom, poziom odwrócony. */
    override fun orientation(index: Int) {
        this.index = index
        if (!pinned) activity.requestedOrientation = requested(index)
    }

    /**
     * Przed zgaszeniem ekranu: przypina obecny obrót. Ekran blokady obraca wygaszony
     * wyświetlacz do pionu, a po zapaleniu czujnik nie zna jeszcze położenia — system
     * stawiał wtedy domyślny poziom i po pół sekundy obracał ekran o 180°.
     */
    fun pinRotation() {
        if (index != 0 && index != 2) return
        pinned = true
        activity.requestedOrientation = when (rotation()) {
            Surface.ROTATION_90 -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
            Surface.ROTATION_180 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT
            Surface.ROTATION_270 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
            else -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        }
    }

    /** Po wybudzeniu, gdy czujnik już działa: orientacja znowu wg ustawień. */
    fun releaseRotation() {
        if (!pinned) return
        pinned = false
        activity.requestedOrientation = requested(index)
    }

    private fun rotation(): Int =
        if (Build.VERSION.SDK_INT >= 30) activity.display?.rotation ?: Surface.ROTATION_0
        else @Suppress("DEPRECATION") activity.windowManager.defaultDisplay.rotation

    private fun requested(index: Int) = when (index) {
        1 -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
        2 -> ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
        3 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
        else -> ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
    }
}
