package app.couchpad.pilot.ui.home

import androidx.activity.compose.BackHandler
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.runtime.Composable
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import app.couchpad.pilot.data.PilotRepository
import app.couchpad.pilot.data.PilotState
import app.couchpad.pilot.data.Preferences
import app.couchpad.pilot.ui.climate.ClimateScreen
import app.couchpad.pilot.ui.components.EdgeDrawer
import app.couchpad.pilot.ui.theme.LocalLandscape

/** Poziomy zakładki Dom z home-screen.md §1. Głębiej niż żarówka nie schodzimy. */
internal sealed class HomeNav {
    object Rooms : HomeNav()
    data class Room(val room: String) : HomeNav()
    data class Light(val room: String, val light: String) : HomeNav()
    data class Bulb(val room: String, val light: String, val bulb: String) : HomeNav()
    data class Climate(val room: String) : HomeNav()
    data class Radiator(val room: String) : HomeNav()

    val parent: HomeNav
        get() = when (this) {
            Rooms, is Room -> Rooms
            is Light -> Room(room)
            is Bulb -> Light(room, light)
            is Climate -> Room(room)
            is Radiator -> Room(room)
        }

    val roomId: String?
        get() = when (this) {
            Rooms -> null
            is Room -> room
            is Light -> room
            is Bulb -> room
            is Climate -> room
            is Radiator -> room
        }
}

/**
 * Zakładka Dom: pokoje → pokój → lampa → żarówka, oraz klimatyzacja i grzejnik pokoju. W poziomie od pokoju
 * w głąb treść zajmuje cały ekran ([onImmersive]): zakładki chowają się do lewej szuflady,
 * pokoje do prawej.
 */
@Composable
fun HomeScreen(state: PilotState, prefs: Preferences, repo: PilotRepository, onImmersive: (Boolean) -> Unit = {}) {
    var nav by remember { mutableStateOf<HomeNav>(HomeNav.Rooms) }
    var roomsDrawer by remember { mutableStateOf(false) }
    val landscape = LocalLandscape.current

    LaunchedEffect(nav, landscape) { onImmersive(landscape && nav != HomeNav.Rooms) }
    BackHandler(enabled = nav != HomeNav.Rooms) { nav = nav.parent }

    val switchRoom: (String) -> Unit = { id -> nav = HomeNav.Room(id); roomsDrawer = false }
    val back = { nav = nav.parent }

    Box(Modifier.fillMaxSize()) {
        when (val n = nav) {
            HomeNav.Rooms -> RoomsGrid(
                state, prefs.roomName, repo,
                open = { nav = HomeNav.Room(it) },
                openClimate = { nav = HomeNav.Climate(ClimateRoom) },
            )
            is HomeNav.Room -> RoomPage(
                n.room, state, repo, back, switchRoom,
                openLight = { nav = HomeNav.Light(n.room, it) },
                openClimate = { nav = HomeNav.Climate(n.room) },
                openRadiator = { nav = HomeNav.Radiator(n.room) },
            )
            is HomeNav.Light -> {
                val light = state.home.light(n.light)
                // Lampa zniknęła ze stanu (np. zmiana w HA) — wracamy poziom wyżej.
                if (light == null) LaunchedEffect(Unit) { nav = n.parent }
                else LightPage(n.room, light, state, repo, back, switchRoom) { nav = HomeNav.Bulb(n.room, n.light, it) }
            }
            is HomeNav.Bulb -> {
                val light = state.home.light(n.light)
                val bulb = light?.members?.firstOrNull { it.id == n.bulb }
                if (light == null || bulb == null) LaunchedEffect(Unit) { nav = n.parent }
                else BulbPage(n.room, light, bulb, state, repo, back, switchRoom)
            }
            is HomeNav.Climate -> ClimateScreen(state, repo, back)
            is HomeNav.Radiator -> {
                // Pokój zmieniony w szufladzie albo grzejnik zniknął ze stanu — poziom wyżej.
                val radiator = state.home.radiatorIn(n.room)
                if (radiator == null) LaunchedEffect(Unit) { nav = n.parent }
                else RadiatorPage(n.room, radiator, state, repo, back, switchRoom)
            }
        }
        val currentRoom = nav.roomId
        if (landscape && currentRoom != null) {
            EdgeDrawer(left = false, open = roomsDrawer, onOpen = { roomsDrawer = it }, width = 132) {
                RoomStrip(currentRoom, vertical = true, onSelect = switchRoom)
            }
        }
    }
}
