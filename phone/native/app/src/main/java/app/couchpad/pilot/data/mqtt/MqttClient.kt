package app.couchpad.pilot.data.mqtt

import android.util.Log
import java.io.BufferedInputStream
import java.io.EOFException
import java.io.InputStream
import java.io.OutputStream
import java.net.InetSocketAddress
import java.net.Socket
import java.net.SocketTimeoutException
import java.util.concurrent.Executors
import java.util.concurrent.atomic.AtomicInteger
import kotlin.concurrent.thread

/**
 * Minimalny klient MQTT 3.1.1 po TCP: CONNECT z testamentem, SUBSCRIBE, PUBLISH QoS 0/1,
 * keepalive i samodzielne ponawianie. Wszystkie wywołania zwrotne przychodzą z wątku klienta.
 *
 * Bez bibliotek: pilot potrzebuje tylko tego podzbioru, a własny klient daje pełną kontrolę
 * nad tym, kiedy łączymy się po wybudzeniu (ADR 0005). Pakiety koduje [MqttCodec].
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
    @Volatile private var lastSent = 0L
    private val writeLock = Any()
    private val packetIds = AtomicInteger(1)
    private val sleeper = Object()

    // Zapis do gniazda nie może iść z wątku UI (Android tego zabrania), więc publikacja
    // trafia do osobnego wątku wysyłki. Dotknięcie nigdy nie czeka na sieć.
    private val sender = Executors.newSingleThreadExecutor { r -> Thread(r, "mqtt-tx").apply { isDaemon = true } }

    fun start() {
        if (running) return
        running = true
        thread(name = "mqtt", isDaemon = true) { run() }
    }

    fun stop() {
        if (!running) return
        val wasConnected = connected
        running = false
        synchronized(sleeper) { sleeper.notifyAll() }
        val s = socket
        sender.execute {
            if (wasConnected) runCatching { send(MqttCodec.disconnect) }
            runCatching { s?.close() }
        }
        sender.shutdown()
    }

    /** Przerywa oczekiwanie na kolejną próbę — np. po powrocie aplikacji na pierwszy plan. */
    fun reconnectNow() {
        synchronized(sleeper) { sleeper.notifyAll() }
    }

    fun publish(topic: String, payload: String, qos: Int = 1, retain: Boolean = false): Boolean {
        if (!connected || !running) return false
        val packet = MqttCodec.publish(topic, payload, qos, retain, nextId())
        // Po stop() wątek wysyłki jest zamknięty — odrzucona wysyłka to brak połączenia.
        return runCatching { sender.execute { runCatching { send(packet) } } }.isSuccess
    }

    fun subscribe(topic: String, qos: Int = 1): Boolean {
        if (!connected) return false
        return runCatching { send(MqttCodec.subscribe(topic, qos, nextId())) }.isSuccess
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
            s.soTimeout = POLL_MILLIS
        } catch (e: Exception) {
            Log.w(TAG, "brak polaczenia z brokerem: ${e.message}")
            runCatching { s.close() }
            return Failure.Unreachable
        }
        socket = s
        output = s.getOutputStream()
        val input = BufferedInputStream(s.getInputStream())

        handshake(input)?.let { return it }
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
                    send(MqttCodec.pingRequest)
                    pingPending = true
                }
                if (now - lastReceived > keepAlive + keepAlive / 2) {
                    Log.w(TAG, "broker milczy, zrywam polaczenie")
                    return Failure.Dropped
                }
                val (header, body) = readPacket(input, Long.MAX_VALUE, poll = true) ?: continue
                lastReceived = System.currentTimeMillis()
                when (header shr 4) {
                    MqttCodec.PUBLISH -> handlePublish(header, body)
                    MqttCodec.PINGRESP -> pingPending = false
                    MqttCodec.PUBREL -> if (body.size >= 2) send(MqttCodec.frame(MqttCodec.PUBCOMP shl 4, body.copyOfRange(0, 2)))
                }
            }
        } catch (e: Exception) {
            if (running) Log.w(TAG, "rozlaczono: ${e.message}")
        }
        return Failure.Dropped
    }

    /** CONNECT i CONNACK; null, gdy broker przyjął połączenie. */
    private fun handshake(input: InputStream): Failure? = try {
        send(
            MqttCodec.connect(
                MqttCodec.Connect(
                    config.clientId, config.keepAliveSeconds, config.username, config.password,
                    config.willTopic, config.willMessage,
                )
            )
        )
        val deadline = System.currentTimeMillis() + config.timeoutMillis
        val packet = readPacket(input, deadline)
        when (val code = packet?.let { MqttCodec.connackCode(it.first, it.second) }) {
            null -> Failure.Unreachable
            0 -> null
            4, 5 -> Failure.NotAuthorized
            else -> Failure.Rejected(code)
        }
    } catch (e: Exception) {
        Failure.Unreachable
    }

    private fun handlePublish(header: Int, body: ByteArray) {
        val message = MqttCodec.parsePublish(header, body) ?: return
        runCatching { listener.onMessage(message.topic, message.payload, message.retained) }
            .onFailure { Log.w(TAG, "blad obslugi ${message.topic}", it) }
        when (message.qos) {
            1 -> send(MqttCodec.ack(MqttCodec.PUBACK, message.packetId))
            2 -> send(MqttCodec.ack(MqttCodec.PUBREC, message.packetId))
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

    private fun send(data: ByteArray) {
        synchronized(writeLock) {
            val out = output ?: throw IllegalStateException("brak polaczenia")
            out.write(data)
            out.flush()
            lastSent = System.currentTimeMillis()
        }
    }

    companion object {
        private const val TAG = "mqtt"
        private const val MAX_PACKET = 1024 * 1024
        private const val POLL_MILLIS = 500
    }
}
