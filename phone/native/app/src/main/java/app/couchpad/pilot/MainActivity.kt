package app.couchpad.pilot

import android.content.Intent
import android.graphics.Color
import android.os.Build
import android.os.Bundle
import android.os.VibrationEffect
import android.os.Vibrator
import android.view.WindowManager
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import app.couchpad.pilot.ui.shell.PilotApp
import app.couchpad.pilot.ui.theme.DarkPalette
import app.couchpad.pilot.ui.theme.Haptics
import androidx.compose.ui.graphics.toArgb

/**
 * Jedyna aktywność: pilot na cały ekran, także jako launcher telefonu (ADR 0010).
 * Po każdym wyjściu na wierzch łączy się od nowa i pobiera świeży stan (ADR 0005).
 */
class MainActivity : ComponentActivity() {
    private val windowDisplay by lazy { WindowDisplay(this) }
    private val releaseRotation = Runnable { windowDisplay.releaseRotation() }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        AppGraph.init(this)
        setupWindow()
        Haptics.tick = vibration()
        setContent { PilotApp(AppGraph.repo, windowDisplay, AppGraph.home) }
    }

    private fun setupWindow() {
        WindowCompat.setDecorFitsSystemWindows(window, false)
        // Launcher: po zapaleniu ekranu pilot pokazuje się od razu, nad ekranem blokady
        // (bez PIN-u nie trzeba go przesuwać). Zabezpieczona blokada nadal prosi o PIN
        // przy wyjściu do innych aplikacji.
        if (Build.VERSION.SDK_INT >= 27) {
            setShowWhenLocked(true)
            setTurnScreenOn(true)
        } else {
            @Suppress("DEPRECATION")
            window.addFlags(
                WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED or
                WindowManager.LayoutParams.FLAG_TURN_SCREEN_ON
            )
        }
        // Rysujemy także obok przedniej kamery: w poziomie pasek zakładek wchodzi w ten pas,
        // a ikonę na wysokości kamery odsuwa (TabBars.kt, SideRail).
        if (Build.VERSION.SDK_INT >= 28) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
        window.statusBarColor = Color.TRANSPARENT
        window.navigationBarColor = DarkPalette.bg.toArgb()
        // Pilot pracuje jako pełnoekranowe urządzenie: bez paska stanu.
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.statusBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    /** Krótkie „tyknięcie” przy dotknięciu kontrolki. */
    private fun vibration(): () -> Unit {
        val vibrator = applicationContext.getSystemService(Vibrator::class.java)
        return {
            if (vibrator?.hasVibrator() == true) {
                when {
                    Build.VERSION.SDK_INT >= 29 -> vibrator.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_TICK))
                    Build.VERSION.SDK_INT >= 26 -> vibrator.vibrate(VibrationEffect.createOneShot(12, 60))
                    else -> @Suppress("DEPRECATION") vibrator.vibrate(12)
                }
            }
        }
    }

    override fun onDestroy() {
        super.onDestroy()
        Haptics.tick = {}
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        // Home przy otwartym pilocie: jak w launcherze — wracamy na ekran startowy.
        if (intent.hasCategory(Intent.CATEGORY_HOME)) AppGraph.goHome()
    }

    override fun onStart() {
        super.onStart()
        // Po każdym wybudzeniu: połączenie i świeży stan (ADR 0005). Rozłączenie w tle
        // oszczędza baterię — ponowne połączenie trwa poniżej sekundy.
        AppGraph.repo.start()
        AppGraph.repo.wake()
        AppGraph.goHome()
        // Czujnik położenia rusza z ekranem; do tego czasu trzymamy obrót sprzed zgaszenia.
        window.decorView.removeCallbacks(releaseRotation)
        window.decorView.postDelayed(releaseRotation, ROTATION_PIN_MS)
    }

    override fun onStop() {
        super.onStop()
        window.decorView.removeCallbacks(releaseRotation)
        windowDisplay.pinRotation()
        AppGraph.repo.stop()
    }
}

/** Jak długo po zapaleniu ekranu trzymać obrót sprzed zgaszenia (czujnik potrzebuje ~0,9 s). */
private const val ROTATION_PIN_MS = 1500L
