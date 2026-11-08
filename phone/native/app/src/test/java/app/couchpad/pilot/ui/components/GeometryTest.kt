package app.couchpad.pilot.ui.components

import androidx.compose.ui.geometry.Offset
import org.junit.Assert.assertEquals
import org.junit.Test

class GeometryTest {
    private val size = 200f

    @Test
    fun `arc runs from bottom left through top to bottom right`() {
        // 135° (lewy dół) to początek, 270° (góra) to połowa, 45° (prawy dół) to koniec.
        assertEquals(0f, ArcGeometry.fractionAt(Offset(29.3f, 170.7f), size), 0.01f)
        assertEquals(0.5f, ArcGeometry.fractionAt(Offset(100f, 0f), size), 0.01f)
        assertEquals(1f, ArcGeometry.fractionAt(Offset(170.7f, 170.7f), size), 0.01f)
    }

    @Test
    fun `gap at the bottom snaps to the nearer end`() {
        assertEquals(1f, ArcGeometry.fractionAt(Offset(120f, 199f), size), 0f)
        assertEquals(0f, ArcGeometry.fractionAt(Offset(80f, 199f), size), 0f)
    }

    @Test
    fun `snap rounds to step from min and clamps`() {
        assertEquals(21.5, ArcGeometry.snap(21.6, 16.0, 31.0, 0.5), 1e-9)
        assertEquals(31.0, ArcGeometry.snap(40.0, 16.0, 31.0, 0.5), 1e-9)
        assertEquals(5.0, ArcGeometry.snap(6.0, 5.0, 100.0, 5.0), 1e-9)
    }

    @Test
    fun `key under finger`() {
        assertEquals("ok", keyAt(Offset(100f, 100f), size, 0.21f, "ok"))
        assertEquals("up", keyAt(Offset(100f, 10f), size, 0.21f, "ok"))
        assertEquals("right", keyAt(Offset(190f, 110f), size, 0.21f, "ok"))
        assertEquals("left", direction(-5f, 2f))
        assertEquals("down", direction(1f, 5f))
    }
}
