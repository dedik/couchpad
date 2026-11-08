package app.couchpad.pilot.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class TvAggregatorTest {
    private val telemetry = """
        {"inputs": {"HDMI_2": "Apple TV", "HDMI_1": "PC"}, "input_source": "HDMI_2",
         "picture": {"modes": [{"value": "cinema", "label": "Kino"}, {"value": ""}], "mode_raw": "cinema"},
         "powerState": {"systemOn": true}}
    """.trimIndent()

    @Test
    fun `telemetry gives inputs with live tv first and picture modes`() {
        val tv = TvAggregator()
        tv.apply("status", "online", 1)
        val s = tv.apply("telemetry", telemetry, 2)!!
        assertEquals(listOf("livetv", "HDMI_1", "HDMI_2"), s.sources.map { it.id })
        assertEquals("Apple TV", s.sourceLabel)
        assertEquals("Kino", s.pictureLabel)
        assertEquals(1, s.pictureModes.size)
        assertTrue(s.power)
        assertTrue(s.available)
    }

    @Test
    fun `immediate topics win over older telemetry`() {
        val tv = TvAggregator()
        tv.apply("status", "online", 1)
        tv.apply("state/application/app", "livetv", 2)
        val s = tv.apply("telemetry", telemetry, 3)!!
        assertEquals("livetv", s.source)
    }

    @Test
    fun `off status means available but not powered`() {
        val tv = TvAggregator()
        val s = tv.apply("status", "off", 1)!!
        assertTrue(s.available)
        assertFalse(s.power)
        assertNull(tv.apply("unknown", "x", 2))
    }

    @Test
    fun `system off overrides online status`() {
        val tv = TvAggregator()
        tv.apply("status", "online", 1)
        assertFalse(tv.apply("state/power/systemOn", "false", 2)!!.power)
    }
}
