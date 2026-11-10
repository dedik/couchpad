package app.couchpad.pilot

import android.content.Context
import app.couchpad.pilot.data.AppUpdater
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.SettingsStore
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

/** Obiekty na cały proces — przeżywają odtworzenie aktywności (obrót, zmiana stylu). */
object AppGraph {
    lateinit var repo: PilotRepository
        private set

    /** Aktualizacja aplikacji przez Wi-Fi (ADR 0020). */
    lateinit var updater: AppUpdater
        private set

    private val _home = MutableStateFlow(0)

    /**
     * Rośnie przy każdym powrocie na ekran startowy: Home, gdy pilot jest ekranem głównym,
     * oraz każde zapalenie ekranu — po wybudzeniu pilot pokazuje Start (ADR 0010).
     */
    val home: StateFlow<Int> = _home.asStateFlow()

    fun init(context: Context) {
        if (!::repo.isInitialized) repo = PilotRepository(SettingsStore(context.applicationContext))
        if (!::updater.isInitialized) updater = AppUpdater(context)
    }

    fun goHome() {
        _home.value++
    }
}
