package app.couchpad.pilot.data

import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class Z906Test {
    @Test
    fun `step moves by one amplifier level`() {
        // 44 poziomy: 50% to poziom 22 → 23 to 53%.
        assertEquals(53, Z906.stepVolume(50, 1, 100))
        assertEquals(49, Z906.stepVolume(51, -1, 100))
    }

    @Test
    fun `step stops at the ends`() {
        assertEquals(100, Z906.stepVolume(100, 1, 100))
        assertEquals(0, Z906.stepVolume(0, -1, 100))
    }

    @Test
    fun `step up never exceeds the limit and never goes down`() {
        // Limit 60% to poziom 25 (58%); krok w górę z 60% nie przekracza limitu i nie obniża.
        assertEquals(58, Z906.stepVolume(55, 1, 60))
        assertEquals(60, Z906.stepVolume(58, 1, 60))
        assertEquals(60, Z906.stepVolume(60, 1, 60))
        assertEquals("nad limitem krok w górę nie obniża", 80, Z906.stepVolume(80, 1, 60))
    }

    @Test
    fun `topic updates state and marks controller available`() {
        val s = Z906State().withTopic("state/volume", "37")!!
        assertEquals(37, s.volume)
        assertTrue(s.available)
        assertEquals(true, Z906State().withTopic("state/power", "on")!!.power)
        assertEquals(42, Z906State().withTopic("state/centre", "42")!!.level(Z906Channel.Center))
    }

    @Test
    fun `availability offline is kept and unknown leaf is ignored`() {
        assertEquals(false, Z906State(available = true).withTopic("availability", "offline")!!.available)
        assertNull(Z906State().withTopic("state/unknown", "1"))
    }
}
