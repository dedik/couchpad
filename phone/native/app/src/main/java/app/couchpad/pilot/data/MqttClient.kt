package app.couchpad.pilot.data

import android.util.Log
import java.io.BufferedInputStream
import java.io.EOFException
import java.io.InputStream
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketTimeoutException
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread

/**
 * Minimalny klient MQTT 3.1.1 po TCP: CONNECT z testamentem, SUBSCRIBE, PUBLISH QoS 0/1,
 * keepalive i samodzielne ponawianie. Wszystkie wywołania zwrotne przychodzą z wątku klienta.
 *
 * Bez bibliotek: pilot potrzebuje tylko tego podzbioru, a własny klient daje pełną kontrolę
 * nad tym, kiedy łączymy się po wybudzeniu (ADR 0005).
 */
class MqttClient(
    private val config: Config,
    private val listener: Listener,
) {
    data class Config(
        val host: String,
        val port: Int,
        val clientId: String,
        val username: String,
        val password: String,
        val willTopic: String?,
        val willMessage: String = "offline",
        val keepAliveSeconds: Int = 60,
        val reconnectMillis: Long = 3000,
        val timeoutMillis: Int = 8000,
    )

    sealed class Failure {
        object Unreachable : Failure()
        object NotAuthorized : Failure()
        data class Rejected(val code: Int) : Failure()
        object Dropped : Failure()
    }

    interface Listener {
        fun onConnected()
        fun onDisconnected(failure: Failure)
        fun onMessage(topic: String, payload: ByteArray, retained: Boolean)
    }

    @Volatile private var running = false
    @Volatile private var socket: Socket? = null
    @Volatile private var output: OutputStream? = null
    @Volatile var connected = false
        private set
    private val writeLock = Any()
    private val packetIds = AtomicInteger(1)
    private val sleeper = Object()
    private var worker: Thread? = null
    @Volatile private var lastSent = 0L

    fun start() {
        if (running) return
        running = true
        worker = thread(name = "mqtt", isDaemon = true) { run() }
    }

    fun stop() {
        if (!running) return
        val wasConnected = connected
        running = false
        synchronized(sleeper) { sleeper.notifyAll() }
        val s = socket
        sender.execute {
            if (wasConnected) runCatching { send(byteArrayOf(0xE0.toByte(), 0)) }
            runCatching { s?.close() }
        }
    }

    /** Przerywa oczekiwanie na kolejną próbę — np. po powrocie aplikacji na pierwszy plan. */
    fun reconnectNow() {
        synchronized(sleeper) { sleeper.notifyAll() }
    }

    // Zapis do gniazda nie może iść z wątku UI (Android tego zabrania), więc publikacja
    // trafia do osobnego wątku wysyłki. Dotknięcie nigdy nie czeka na sieć.
    private val sender = java.util.concurrent.Executors.newSingleThreadExecutor { r ->
        Thread(r, "mqtt-tx").apply { isDaemon = true }
    }

    fun publish(topic: String, payload: String, qos: Int = 1, retain: Boolean = false): Boolean {
        if (!connected) return false
        val body = Buf()
        body.string(topic)
        val q = qos.coerceIn(0, 1)
        if (q > 0) body.u16(nextId())
        body.bytes(payload.toByteArray(Charsets.UTF_8))
        val header = (3 shl 4) or (q shl 1) or (if (retain) 1 else 0)
        val packet = frame(header, body.toByteArray())
        sender.execute { runCatching { send(packet) } }
        return true
    }

    fun subscribe(topic: String, qos: Int = 1): Boolean {
        if (!connected) return false
        val body = Buf()
        body.u16(nextId())
        body.string(topic)
        body.byte(qos.coerceIn(0, 1))
        return runCatching { send(frame((8 shl 4) or 0x02, body.toByteArray())) }.isSuccess
    }

    private fun nextId(): Int {
        var id = packetIds.getAndIncrement() and 0xFFFF
        if (id == 0) id = packetIds.getAndIncrement() and 0xFFFF
        return id
    }

    private fun run() {
        while (running) {
            val failure = session()
            connected = false
            runCatching { socket?.close() }
            socket = null
            output = null
            if (!running) break
            listener.onDisconnected(failure)
            synchronized(sleeper) {
                if (running) sleeper.wait(config.reconnectMillis)
            }
        }
    }

    /** Jedna sesja: od połączenia TCP do jego utraty. Zwraca powód końca. */
    private fun session(): Failure {
        val s = Socket()
        try {
            s.tcpNoDelay = true
            s.connect(InetSocketAddress(config.host, config.port), config.timeoutMillis)
            s.soTimeout = 500
        } catch (e: Exception) {
            Log.w(TAG, "brak polaczenia z brokerem: ${e.message}")
            runCatching { s.close() }
            return Failure.Unreachable
        }
        socket = s
        output = s.getOutputStream()
        val input = BufferedInputStream(s.getInputStream())

        try {
            send(connectPacket())
            val deadline = System.currentTimeMillis() + config.timeoutMillis
            val (type, body) = readPacket(input, deadline) ?: return Failure.Unreachable
            if (type shr 4 != 2 || body.size < 2) return Failure.Unreachable
            when (val code = body[1].toInt() and 0xFF) {
                0 -> Unit
                4, 5 -> return Failure.NotAuthorized
                else -> return Failure.Rejected(code)
            }
        } catch (e: Exception) {
            return Failure.Unreachable
        }

        connected = true
        Log.i(TAG, "polaczono z brokerem")
        listener.onConnected()

        var lastReceived = System.currentTimeMillis()
        var pingPending = false
        val keepAlive = config.keepAliveSeconds * 1000L
        try {
            while (running) {
                val now = System.currentTimeMillis()
                if (now - lastSent >= keepAlive / 2 && !pingPending) {
                    send(byteArrayOf(0xC0.toByte(), 0))
                    pingPending = true
                }
                if (now - lastReceived > keepAlive + keepAlive / 2) {
                    Log.w(TAG, "broker milczy, zrywam polaczenie")
                    return Failure.Dropped
                }
                val packet = readPacket(input, Long.MAX_VALUE, poll = true) ?: continue
                lastReceived = System.currentTimeMillis()
                val (header, body) = packet
                when (header shr 4) {
                    3 -> handlePublish(header, body)
                    13 -> pingPending = false
                    6 -> if (body.size >= 2) send(frame(7 shl 4, body.copyOfRange(0, 2)))
                    else -> Unit
                }
            }
        } catch (e: Exception) {
            if (running) Log.w(TAG, "rozlaczono: ${e.message}")
        }
        return Failure.Dropped
    }

    private fun handlePublish(header: Int, body: ByteArray) {
        val qos = (header shr 1) and 3
        if (body.size < 2) return
        val topicLength = ((body[0].toInt() and 0xFF) shl 8) or (body[1].toInt() and 0xFF)
        var offset = 2 + topicLength
        if (offset > body.size) return
        val topic = String(body, 2, topicLength, Charsets.UTF_8)
        var packetId = 0
        if (qos > 0) {
            packetId = ((body[offset].toInt() and 0xFF) shl 8) or (body[offset + 1].toInt() and 0xFF)
            offset += 2
        }
        val payload = body.copyOfRange(offset, body.size)
        runCatching { listener.onMessage(topic, payload, header and 1 != 0) }
            .onFailure { Log.w(TAG, "blad obslugi $topic", it) }
        if (qos == 1) {
            send(frame(4 shl 4, byteArrayOf((packetId shr 8).toByte(), packetId.toByte())))
        } else if (qos == 2) {
            send(frame(5 shl 4, byteArrayOf((packetId shr 8).toByte(), packetId.toByte())))
        }
    }

    /**
     * Czyta jeden pakiet. Przy `poll` brak pierwszego bajtu w czasie soTimeout zwraca null
     * (pętla sesji sprawdza wtedy keepalive); reszta pakietu ma limit `timeoutMillis`.
     */
    private fun readPacket(input: InputStream, deadline: Long, poll: Boolean = false): Pair<Int, ByteArray>? {
        var first: Int
        while (true) {
            try {
                first = input.read()
                break
            } catch (e: SocketTimeoutException) {
                if (poll || System.currentTimeMillis() > deadline || !running) return null
            }
        }
        if (first < 0) throw EOFException()
        val bodyDeadline = System.currentTimeMillis() + config.timeoutMillis
        var length = 0
        var multiplier = 1
        for (i in 0 until 4) {
            val b = readByte(input, bodyDeadline)
            length += (b and 0x7F) * multiplier
            multiplier *= 128
            if (b and 0x80 == 0) break
        }
        if (length > MAX_PACKET) throw IllegalStateException("za duza wiadomosc: $length")
        val body = ByteArray(length)
        var got = 0
        while (got < length) {
            try {
                val n = input.read(body, got, length - got)
                if (n < 0) throw EOFException()
                got += n
            } catch (e: SocketTimeoutException) {
                if (System.currentTimeMillis() > bodyDeadline) throw e
            }
        }
        return first to body
    }

    private fun readByte(input: InputStream, deadline: Long): Int {
        while (true) {
            try {
                val b = input.read()
                if (b < 0) throw EOFException()
                return b
            } catch (e: SocketTimeoutException) {
                if (System.currentTimeMillis() > deadline) throw e
            }
        }
    }

    private fun connectPacket(): ByteArray {
        val body = Buf()
        body.string("MQTT")
        body.byte(4)
        var flags = 0x02
        if (config.willTopic != null) flags = flags or 0x04 or (1 shl 3) or 0x20
        if (config.username.isNotEmpty()) {
            flags = flags or 0x80
            if (config.password.isNotEmpty()) flags = flags or 0x40
        }
        body.byte(flags)
        body.u16(config.keepAliveSeconds)
        body.string(config.clientId)
        if (config.willTopic != null) {
            body.string(config.willTopic)
            body.string(config.willMessage)
        }
        if (config.username.isNotEmpty()) {
            body.string(config.username)
            if (config.password.isNotEmpty()) body.string(config.password)
        }
        return frame(1 shl 4, body.toByteArray())
    }

    private fun send(data: ByteArray) {
        synchronized(writeLock) {
            val out = output ?: throw IllegalStateException("brak polaczenia")
            out.write(data)
            out.flush()
            lastSent = System.currentTimeMillis()
        }
    }

    private fun frame(header: Int, body: ByteArray): ByteArray {
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

    private class Buf {
        private val data = java.io.ByteArrayOutputStream()
        fun byte(v: Int) = data.write(v and 0xFF)
        fun u16(v: Int) { byte(v shr 8); byte(v) }
        fun bytes(b: ByteArray) = data.write(b)
        fun string(s: String) { val b = s.toByteArray(Charsets.UTF_8); u16(b.size); bytes(b) }
        fun toByteArray(): ByteArray = data.toByteArray()
    }

    companion object {
        private const val TAG = "mqtt"
        private const val MAX_PACKET = 1024 * 1024
    }
}
