package app.couchpad.pilot.ui.shell

import app.couchpad.pilot.data.Options
import app.couchpad.pilot.data.Preferences
import org.junit.Assert.assertEquals
import org.junit.Assert.assertFalse
import org.junit.Assert.assertTrue
import org.junit.Test

class ScreenPolicyTest {
    @Test
    fun `theme by hours wraps around midnight`() {
        val prefs = Preferences(theme = 2, themeHours = 1) // 18–8
        assertTrue(ScreenPolicy.dark(prefs, 18))
        assertTrue(ScreenPolicy.dark(prefs, 3))
        assertFalse(ScreenPolicy.dark(prefs, 8))
        assertTrue(ScreenPolicy.dark(Preferences(theme = 0), 12))
        assertFalse(ScreenPolicy.dark(Preferences(theme = 1), 23))
    }

    @Test
    fun `sleep and dim follow the timeout`() {
        val prefs = Preferences(timeout = 1, dimBeforeSleep = true) // 30 s
        assertFalse(ScreenPolicy.shouldSleep(prefs, 29_000))
        assertTrue(ScreenPolicy.shouldSleep(prefs, 31_000))
        assertFalse(ScreenPolicy.dimmed(prefs, 19_000))
        assertTrue(ScreenPolicy.dimmed(prefs, 21_000))
        val never = Preferences(timeout = Options.timeouts.indexOf(0L))
        assertFalse(ScreenPolicy.shouldSleep(never, Long.MAX_VALUE / 2))
    }

    @Test
    fun `brightness is capped at night, when dimmed and asleep`() {
        val prefs = Preferences(brightness = 80, night = 1) // 21–7
        assertEquals(80, ScreenPolicy.brightness(prefs, 12, asleep = false, dimmed = false))
        assertEquals(Options.nightBrightness, ScreenPolicy.brightness(prefs, 23, asleep = false, dimmed = false))
        assertEquals(ScreenPolicy.DIM_PERCENT, ScreenPolicy.brightness(prefs, 12, asleep = false, dimmed = true))
        assertEquals(ScreenPolicy.ASLEEP_PERCENT, ScreenPolicy.brightness(prefs, 12, asleep = true, dimmed = false))
    }

    @Test
    fun `hour window`() {
        assertFalse(Options.hourIn(0 to 0, 0))
        assertTrue(Options.hourIn(9 to 17, 9))
        assertFalse(Options.hourIn(9 to 17, 17))
        assertTrue(Options.hourIn(23 to 6, 2))
    }
}
