package app.couchpad.pilot.data.mqtt

import java.io.ByteArrayOutputStream

/**
 * Kodowanie i dekodowanie pakietów MQTT 3.1.1 potrzebnych pilotowi. Czyste funkcje bez
 * gniazd — [MqttClient] tylko je wysyła i czyta.
 */
internal object MqttCodec {
    const val CONNECT = 1
    const val CONNACK = 2
    const val PUBLISH = 3
    const val PUBACK = 4
    const val PUBREC = 5
    const val PUBREL = 6
    const val PUBCOMP = 7
    const val SUBSCRIBE = 8
    const val PINGRESP = 13

    val pingRequest = byteArrayOf(0xC0.toByte(), 0)
    val disconnect = byteArrayOf(0xE0.toByte(), 0)

    data class Connect(
        val clientId: String,
        val keepAliveSeconds: Int,
        val username: String = "",
        val password: String = "",
        val willTopic: String? = null,
        val willMessage: String = "offline",
    )

    /** CONNECT z czystą sesją; testament z QoS 1 i retain. */
    fun connect(c: Connect): ByteArray {
        val body = Buf()
        body.string("MQTT")
        body.byte(4)
        var flags = 0x02
        if (c.willTopic != null) flags = flags or 0x04 or (1 shl 3) or 0x20
        if (c.username.isNotEmpty()) {
            flags = flags or 0x80
            if (c.password.isNotEmpty()) flags = flags or 0x40
        }
        body.byte(flags)
        body.u16(c.keepAliveSeconds)
        body.string(c.clientId)
        if (c.willTopic != null) {
            body.string(c.willTopic)
            body.string(c.willMessage)
        }
        if (c.username.isNotEmpty()) {
            body.string(c.username)
            if (c.password.isNotEmpty()) body.string(c.password)
        }
        return frame(CONNECT shl 4, body.toByteArray())
    }

    /** Kod powrotu z CONNACK albo null, gdy pakiet nie jest poprawnym CONNACK. */
    fun connackCode(header: Int, body: ByteArray): Int? =
        if (header shr 4 != CONNACK || body.size < 2) null else body[1].toInt() and 0xFF

    fun publish(topic: String, payload: String, qos: Int, retain: Boolean, packetId: Int): ByteArray {
        val q = qos.coerceIn(0, 1)
        val body = Buf()
        body.string(topic)
        if (q > 0) body.u16(packetId)
        body.bytes(payload.toByteArray(Charsets.UTF_8))
        val header = (PUBLISH shl 4) or (q shl 1) or (if (retain) 1 else 0)
        return frame(header, body.toByteArray())
    }

    fun subscribe(topic: String, qos: Int, packetId: Int): ByteArray {
        val body = Buf()
        body.u16(packetId)
        body.string(topic)
        body.byte(qos.coerceIn(0, 1))
        return frame((SUBSCRIBE shl 4) or 0x02, body.toByteArray())
    }

    /** Potwierdzenie (PUBACK, PUBREC, PUBCOMP) dla identyfikatora pakietu. */
    fun ack(type: Int, packetId: Int): ByteArray =
        frame(type shl 4, byteArrayOf((packetId shr 8).toByte(), packetId.toByte()))

    data class Incoming(val topic: String, val payload: ByteArray, val qos: Int, val retained: Boolean, val packetId: Int)

    /** Treść przychodzącego PUBLISH albo null, gdy pakiet jest uszkodzony. */
    fun parsePublish(header: Int, body: ByteArray): Incoming? {
        val qos = (header shr 1) and 3
        if (body.size < 2) return null
        val topicLength = u16(body, 0)
        var offset = 2 + topicLength
        if (offset > body.size) return null
        val topic = String(body, 2, topicLength, Charsets.UTF_8)
        var packetId = 0
        if (qos > 0) {
            if (offset + 2 > body.size) return null
            packetId = u16(body, offset)
            offset += 2
        }
        return Incoming(topic, body.copyOfRange(offset, body.size), qos, header and 1 != 0, packetId)
    }

    /** Nagłówek stały: typ i flagi, długość pozostała (do 4 bajtów), treść. */
    fun frame(header: Int, body: ByteArray): ByteArray {
        val out = Buf()
        out.byte(header)
        var length = body.size
        do {
            var b = length % 128
            length /= 128
            if (length > 0) b = b or 0x80
            out.byte(b)
        } while (length > 0)
        out.bytes(body)
        return out.toByteArray()
    }

    private fun u16(b: ByteArray, at: Int) = ((b[at].toInt() and 0xFF) shl 8) or (b[at + 1].toInt() and 0xFF)

    private class Buf {
        private val data = ByteArrayOutputStream()
        fun byte(v: Int) = data.write(v and 0xFF)
        fun u16(v: Int) { byte(v shr 8); byte(v) }
        fun bytes(b: ByteArray) = data.write(b)
        fun string(s: String) { val b = s.toByteArray(Charsets.UTF_8); u16(b.size); bytes(b) }
        fun toByteArray(): ByteArray = data.toByteArray()
    }
}
