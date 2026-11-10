package app.couchpad.pilot.ui.remote

import app.couchpad.pilot.data.Option
import app.couchpad.pilot.data.TvState
import org.junit.Assert.assertEquals
import org.junit.Test

class RemoteSourcesTest {
    private val sources = listOf(
        Option("livetv", "Telewizja"),
        Option("HDMI_1", "HDMI 1"),
        Option("HDMI_2", "HDMI 2"),
        Option("HDMI_3", "PC"),
        Option("HDMI_4", "Apple TV"),
    )

    @Test
    fun currentInputComesFirst() {
        val tv = TvState(sources = sources, source = "livetv")
        assertEquals(listOf("livetv", "HDMI_3", "HDMI_4"), remoteSources(tv).map { it.id })
    }

    @Test
    fun hidesInputsWithDefaultNames() {
        val tv = TvState(sources = sources, source = "HDMI_4")
        assertEquals(listOf("HDMI_4", "livetv", "HDMI_3"), remoteSources(tv).map { it.id })
    }

    @Test
    fun keepsCurrentInputEvenWithDefaultName() {
        val tv = TvState(sources = sources, source = "HDMI_1")
        assertEquals(listOf("HDMI_1", "livetv", "HDMI_3", "HDMI_4"), remoteSources(tv).map { it.id })
    }
}
