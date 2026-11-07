package app.couchpad.pilot

import android.content.Intent
import android.content.pm.ActivityInfo
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
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.SettingsStore
import app.couchpad.pilot.ui.DisplayControl
import app.couchpad.pilot.ui.PilotApp
import app.couchpad.pilot.ui.haptic

/** Jedno repozytorium na proces — przeżywa odtworzenie aktywności. */
object Graph {
    lateinit var repo: PilotRepository
    /**
     * Rośnie przy każdym powrocie na ekran startowy: Home, gdy pilot jest ekranem głównym,
     * oraz każde zapalenie ekranu — po wybudzeniu pilot pokazuje Start (ADR 0010).
     */
    val homePressed = kotlinx.coroutines.flow.MutableStateFlow(0)
    fun init(activity: ComponentActivity) {
        if (!::repo.isInitialized) repo = PilotRepository(SettingsStore(activity.applicationContext))
    }
}

class MainActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        Graph.init(this)
        WindowCompat.setDecorFitsSystemWindows(window, false)
        // Launcher: po zapaleniu ekranu pilot pokazuje się od razu, nad ekranem blokady
        // (bez PIN-u nie trzeba go przesuwać). Zabezpieczona blokada nadal prosi o PIN
        // przy wyjściu do innych aplikacji.
        if (Build.VERSION.SDK_INT >= 27) {
            setShowWhenLocked(true)
        } else {
            @Suppress("DEPRECATION")
            window.addFlags(WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED)
        }
        // Rysujemy także obok przedniej kamery: w poziomie pasek zakładek wchodzi w ten pas,
        // a ikonę na wysokości kamery odsuwa (Shell.kt, SideRail).
        if (Build.VERSION.SDK_INT >= 28) {
            window.attributes = window.attributes.apply {
                layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
            }
        }
        window.statusBarColor = android.graphics.Color.TRANSPARENT
        window.navigationBarColor = 0xFF0A0B0E.toInt()
        // Pilot pracuje jako pełnoekranowe urządzenie: bez paska stanu.
        WindowInsetsControllerCompat(window, window.decorView).apply {
            hide(WindowInsetsCompat.Type.statusBars())
            systemBarsBehavior = WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }

        val vibrator = getSystemService(Vibrator::class.java)
        haptic = {
            if (vibrator?.hasVibrator() == true) {
                if (Build.VERSION.SDK_INT >= 29) {
                    vibrator.vibrate(VibrationEffect.createPredefined(VibrationEffect.EFFECT_TICK))
                } else if (Build.VERSION.SDK_INT >= 26) {
                    vibrator.vibrate(VibrationEffect.createOneShot(12, 60))
                } else {
                    @Suppress("DEPRECATION") vibrator.vibrate(12)
                }
            }
        }

        setContent { PilotApp(Graph.repo, display) }
    }

    /** To, czym na płytce steruje `board`: podświetlenie, wygaszanie i orientacja. */
    private val display = object : DisplayControl {
        override fun keepScreenOn(on: Boolean) {
            if (on) window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
            else window.clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        }

        override fun brightness(percent: Int) {
            val attrs = window.attributes
            val value = (percent.coerceIn(1, 100) / 100f)
            if (attrs.screenBrightness != value) {
                attrs.screenBrightness = value
                window.attributes = attrs
            }
        }

        override fun orientation(index: Int) {
            requestedOrientation = when (index) {
                1 -> ActivityInfo.SCREEN_ORIENTATION_PORTRAIT
                2 -> ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                3 -> ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE
                else -> ActivityInfo.SCREEN_ORIENTATION_FULL_SENSOR
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        // Home przy otwartym pilocie: jak w launcherze — wracamy na ekran startowy.
        if (intent.hasCategory(Intent.CATEGORY_HOME)) Graph.homePressed.value++
    }

    override fun onStart() {
        super.onStart()
        // Po każdym wybudzeniu: połączenie i świeży stan (ADR 0005). Rozłączenie w tle
        // oszczędza baterię — ponowne połączenie trwa poniżej sekundy.
        Graph.repo.start()
        Graph.repo.wake()
        Graph.homePressed.value++
    }

    override fun onStop() {
        super.onStop()
        Graph.repo.stop()
    }
}
