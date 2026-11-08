package app.couchpad.pilot.data.mqtt

import org.junit.Assert.assertArrayEquals
import org.junit.Assert.assertEquals
import org.junit.Assert.assertNull
import org.junit.Assert.assertTrue
import org.junit.Test

class MqttCodecTest {
    /** Długość pozostała z nagłówka stałego (bez pierwszego bajtu). */
    private fun remainingLength(packet: ByteArray): Pair<Int, Int> {
        var length = 0
        var multiplier = 1
        var i = 1
        while (true) {
            val b = packet[i].toInt() and 0xFF
            length += (b and 0x7F) * multiplier
            multiplier *= 128
            i++
            if (b and 0x80 == 0) break
        }
        return length to i
    }

    @Test
    fun `remaining length uses variable byte encoding`() {
        for (size in listOf(0, 127, 128, 16383, 16384)) {
            val packet = MqttCodec.frame(0x30, ByteArray(size))
            val (length, headerSize) = remainingLength(packet)
            assertEquals(size, length)
            assertEquals(size + headerSize, packet.size)
        }
        assertEquals(2, remainingLength(MqttCodec.frame(0x30, ByteArray(127))).second)
        assertEquals(3, remainingLength(MqttCodec.frame(0x30, ByteArray(128))).second)
    }

    @Test
    fun `publish round trips through the parser`() {
        val packet = MqttCodec.publish("z906/x/command/volume", "37", qos = 1, retain = true, packetId = 0x1234)
        val header = packet[0].toInt() and 0xFF
        assertEquals(MqttCodec.PUBLISH, header shr 4)
        val (_, start) = remainingLength(packet)
        val message = MqttCodec.parsePublish(header, packet.copyOfRange(start, packet.size))!!
        assertEquals("z906/x/command/volume", message.topic)
        assertEquals("37", String(message.payload))
        assertEquals(1, message.qos)
        assertTrue(message.retained)
        assertEquals(0x1234, message.packetId)
    }

    @Test
    fun `qos 0 publish has no packet id`() {
        val packet = MqttCodec.publish("t", "p", qos = 0, retain = false, packetId = 7)
        // nagłówek, długość, 2 B długości tematu, temat, treść
        assertEquals(2 + 2 + 1 + 1, packet.size)
    }

    @Test
    fun `connect sets will and credential flags`() {
        val packet = MqttCodec.connect(MqttCodec.Connect("pilot", 60, "user", "pass", willTopic = "a/availability"))
        val (_, start) = remainingLength(packet)
        // „MQTT”, poziom 4, potem flagi.
        val flags = packet[start + 6 + 1].toInt() and 0xFF
        assertEquals(0x02 or 0x04 or 0x08 or 0x20 or 0x40 or 0x80, flags)
        val anonymous = MqttCodec.connect(MqttCodec.Connect("pilot", 60))
        assertEquals(0x02, anonymous[remainingLength(anonymous).second + 7].toInt() and 0xFF)
    }

    @Test
    fun `connack and ack`() {
        assertEquals(0, MqttCodec.connackCode(0x20, byteArrayOf(0, 0)))
        assertEquals(5, MqttCodec.connackCode(0x20, byteArrayOf(0, 5)))
        assertNull(MqttCodec.connackCode(0x30, byteArrayOf(0, 0)))
        assertArrayEquals(byteArrayOf(0x40, 2, 0x12, 0x34), MqttCodec.ack(MqttCodec.PUBACK, 0x1234))
    }

    @Test
    fun `truncated publish is rejected`() {
        assertNull(MqttCodec.parsePublish(0x30, byteArrayOf(0)))
        assertNull(MqttCodec.parsePublish(0x32, byteArrayOf(0, 1, 'a'.code.toByte(), 0)))
    }
}
