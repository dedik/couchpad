package app.couchpad.pilot.data

import org.junit.Assert.assertEquals
import org.junit.Test

class EffectUsageTest {
    private val effects = listOf("Cozy", "Relax", "TV time", "Candlelight", "Night light", "Bedtime")

    @Test
    fun `no usage keeps Home Assistant order`() {
        assertEquals(listOf("Cozy", "Relax", "TV time"), EffectUsage.frequent(effects, emptyMap(), "", 3))
    }

    @Test
    fun `most used first, ties by list order, current excluded`() {
        val usage = mapOf("Bedtime" to 4, "Night light" to 4, "Cozy" to 9, "Relax" to 1)
        assertEquals(listOf("Night light", "Bedtime", "Relax"), EffectUsage.frequent(effects, usage, "Cozy", 3))
    }

    @Test
    fun `fewer effects than slots`() {
        assertEquals(listOf("Relax"), EffectUsage.frequent(listOf("Cozy", "Relax"), emptyMap(), "Cozy", 4))
    }
}
