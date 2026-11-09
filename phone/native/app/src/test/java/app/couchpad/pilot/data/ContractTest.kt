package app.couchpad.pilot.data

import org.json.JSONObject
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class ContractTest {
    private val snapshot = """
        {"revision": 12,
         "appletv": {"available": true, "power": "on", "app_id": "a", "apps": [{"id": "a", "label": "Infuse"}],
                     "keyboard_active": true},
         "climate": {"available": true, "hvac_mode": "cool", "hvac_modes": ["off", "cool"], "target": 21.5,
                     "current": null, "min": 17, "max": 30, "step": 1, "fan_modes": ["auto"], "fan_mode": "auto"},
         "lights": [{"room": "salon", "id": "light.lampa", "label": "Lampa", "available": true, "on": true,
                     "brightness": 50, "kelvin": 3000, "min_kelvin": 2200, "max_kelvin": 6500,
                     "effects": ["Cozy"], "members": [{"id": "light.lewa", "label": "Lewa", "available": true,
                     "on": false, "brightness": 0, "kelvin": 0}]}]}
    """.trimIndent()

    @Test
    fun `snapshot parses every part`() {
        val home = Contract.parseSnapshot(snapshot, receivedAt = 5)!!
        assertEquals(12, home.revision)
        assertEquals(5, home.receivedAt)
        assertEquals("Infuse", home.appleTv.appLabel)
        assertTrue(home.appleTv.keyboardActive)
        assertEquals(21.5, home.climate.target!!, 0.0)
        assertNull("null w JSON to brak wartości", home.climate.current)
        assertEquals(1.0, home.climate.step, 0.0)
        assertTrue(home.climate.on)
        val lamp = home.light("light.lampa")!!
        assertTrue(lamp.dimmable)
        assertTrue(lamp.lit)
        assertEquals(listOf("Cozy"), lamp.effects)
        assertEquals("Lewa", lamp.members.single().label)
        assertEquals(listOf(lamp), home.lightsIn("salon"))
    }

    @Test
    fun `missing parts fall back to defaults and bad json is rejected`() {
        val home = Contract.parseSnapshot("""{"revision": 3}""", 1)!!
        assertFalse(home.appleTv.available)
        assertEquals("off", home.climate.hvacMode)
        assertTrue(home.lights.isEmpty())
        assertTrue("bridge sprzed ADR 0018: bez kart grzejników", home.radiators.isEmpty())
        assertNull(Contract.parseSnapshot("nie json", 1))
    }

    @Test
    fun `radiators parse optional fields and keep at most four`() {
        val home = Contract.parseSnapshot(
            """{"revision": 4, "radiators": [
                {"room": "salon", "id": "grzejnik_salon", "available": true, "mode": "auto",
                 "modes": ["off", "auto", "heat"], "target": 19.0, "min": 4.0, "max": 35.0, "step": 0.5,
                 "heating": false, "battery": 100, "window_open": false, "next": {"at": "23:00", "target": 16.0}},
                {"room": "sypialnia", "id": "g2", "available": true, "mode": "heat", "target": 20.5, "heating": true,
                 "window_open": true},
                {"room": "lazienka", "id": "g3", "mode": "off", "heating": true},
                {"room": "biuro", "id": "g4"}, {"room": "kuchnia", "id": "g5"}, {"id": "bez_pokoju"}]}""", 1,
        )!!
        assertEquals(4, home.radiators.size)
        val salon = home.radiatorIn("salon")!!
        assertEquals(RadiatorNext("23:00", 16.0), salon.next)
        assertEquals(100, salon.battery)
        assertEquals(false, salon.windowOpen)
        assertEquals(listOf("off", "auto", "heat"), salon.modes)
        assertFalse(salon.heats)
        val bed = home.radiator("g2")!!
        assertNull(bed.battery)
        assertNull(bed.next)
        assertEquals(0.5, bed.step, 0.0)
        assertFalse("otwarte okno: nie grzeje", bed.heats)
        val bath = home.radiator("g3")!!
        assertNull("pokój bez kontaktronu", bath.windowOpen)
        assertFalse("wyłączona nie grzeje", bath.heats)
        assertNull(home.radiatorIn("kuchnia"))
    }

    @Test
    fun `weather keeps indoor humidity optional`() {
        val w = Contract.parseWeather("""{"condition": "sunny", "temperature": 7.4, "indoor": {"temperature": 22.1}}""", 9)!!
        assertEquals("sunny", w.condition)
        assertEquals(22.1, w.indoorTemperature!!, 0.0)
        assertNull(w.indoorHumidity)
        assertTrue(w.current(9 + 3600_000L))
        assertFalse("pogoda starsza niż 6 h", w.current(9 + 7 * 3600_000L))
    }

    @Test
    fun `room readings in weather`() {
        val w = Contract.parseWeather(
            """{"condition": "sunny", "temperature": 7.4, "indoor": {"temperature": 22.1, "rooms": [
                {"room": "salon", "temperature": 22.6, "humidity": 39},
                {"room": "biuro", "humidity": 38}, {"room": "kuchnia"}, {"temperature": 1}]}}""", 9,
        )!!
        assertEquals(RoomReading("salon", 22.6, 39), w.room("salon"))
        assertEquals(RoomReading("biuro", null, 38), w.room("biuro"))
        assertNull("pokój bez odczytu nie ma karty", w.room("kuchnia"))
        assertNull(w.room("lazienka"))
        assertEquals(3, w.rooms.size)
    }

    @Test
    fun `channels and command body`() {
        val list = Contract.parseChannels("""{"channels": [{"number": "1", "name": "TVP 1", "id": "x"}]}""")!!
        assertEquals(Channel("1", "TVP 1", "x"), list.single())
        val body = JSONObject(Contract.command("id-1", "light.power.set", JSONObject().put("on", true)))
        assertEquals("id-1", body.getString("id"))
        assertEquals("light.power.set", body.getString("action"))
        assertTrue(body.getJSONObject("args").getBoolean("on"))
    }

    @Test
    fun `discovery fills only empty ids`() {
        val empty = BrokerSettings(host = "h")
        assertEquals("z906_abc", PilotRepository.discover(empty, "z906/z906_abc/availability")!!.z906Id)
        assertEquals("pilot1", PilotRepository.discover(empty, "multimedia_remote/v1/pilot1/state")!!.remoteId)
        assertNull(PilotRepository.discover(empty.copy(z906Id = "z906_x"), "z906/z906_abc/availability"))
        assertNull(PilotRepository.discover(empty, "z906/z906_abc/state/volume"))
    }

    @Test
    fun `pending prefixes clear whole device`() {
        val pending = mapOf(Pending.Z906.volume to 1L, Pending.Light.of("a") to 2L, Pending.Tv.source to 3L)
        assertEquals(setOf(Pending.Tv.source), pending.withoutPrefixes(Pending.Z906.prefix, Pending.Light.prefix).keys)
    }
}
