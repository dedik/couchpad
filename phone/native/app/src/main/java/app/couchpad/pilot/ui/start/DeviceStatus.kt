package app.couchpad.pilot.ui.start

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.wifi.WifiManager
import android.os.BatteryManager
import android.os.Build
import androidx.compose.foundation.Canvas
import androidx.compose.foundation.background
import androidx.compose.foundation.border
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.foundation.shape.CircleShape
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.LaunchedEffect
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.remember
import androidx.compose.runtime.setValue
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import androidx.compose.ui.geometry.CornerRadius
import androidx.compose.ui.geometry.Offset
import androidx.compose.ui.geometry.Size
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.Path
import androidx.compose.ui.graphics.StrokeCap
import androidx.compose.ui.graphics.drawscope.Stroke
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.delay

/**
 * Pigułka stanu nad zegarem: siła Wi-Fi i bateria telefonu (makieta docs/ux/mockup-status,
 * wariant B). Dane są lokalne — bez nowych tematów MQTT. Odczyt trwa tylko, dopóki ekran
 * startowy jest w kompozycji.
 */
data class DeviceStatus(
    /** 0 = brak Wi-Fi, 1–3 = poziom sygnału. */
    val wifiLevel: Int = 0,
    /** Telefon w ogóle nie ma sieci (ani Wi-Fi, ani komórkowej). */
    val offline: Boolean = true,
    val battery: Int? = null,
    val charging: Boolean = false,
)

private const val BATTERY_LOW = 15
private const val REFRESH_MS = 10_000L

/** Te same progi co panel (screen_start.cpp). */
private fun levelOf(rssi: Int): Int = when {
    rssi >= -60 -> 3
    rssi >= -70 -> 2
    else -> 1
}

private fun readNetwork(context: Context): Pair<Int, Boolean> {
    val cm = context.getSystemService(ConnectivityManager::class.java) ?: return 0 to true
    val caps = cm.getNetworkCapabilities(cm.activeNetwork) ?: return 0 to true
    if (!caps.hasTransport(NetworkCapabilities.TRANSPORT_WIFI)) return 0 to false
    val rssi = if (Build.VERSION.SDK_INT >= 29 && caps.signalStrength != NetworkCapabilities.SIGNAL_STRENGTH_UNSPECIFIED) {
        caps.signalStrength
    } else {
        @Suppress("DEPRECATION")
        context.applicationContext.getSystemService(WifiManager::class.java)?.connectionInfo?.rssi ?: -127
    }
    // -127 = sterownik nie podał wartości; połączenie jest, więc pokazujemy jeden łuk zamiast nic.
    return levelOf(rssi) to false
}

private fun readBattery(intent: Intent?): Pair<Int?, Boolean> {
    if (intent == null) return null to false
    val level = intent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
    val scale = intent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
    val status = intent.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
    val percent = if (level >= 0 && scale > 0) (level * 100 + scale / 2) / scale else null
    val charging = status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL
    return percent to charging
}

@Composable
fun rememberDeviceStatus(): DeviceStatus {
    val context = LocalContext.current
    var status by remember { mutableStateOf(DeviceStatus()) }

    fun refreshNetwork() {
        val (level, offline) = readNetwork(context)
        status = status.copy(wifiLevel = level, offline = offline)
    }

    DisposableEffect(context) {
        val receiver = object : BroadcastReceiver() {
            override fun onReceive(c: Context, intent: Intent) {
                val (percent, charging) = readBattery(intent)
                status = status.copy(battery = percent, charging = charging)
            }
        }
        // Wiadomość systemowa i „sticky”: rejestracja od razu oddaje bieżący stan.
        val sticky = context.registerReceiver(receiver, IntentFilter(Intent.ACTION_BATTERY_CHANGED))
        val (percent, charging) = readBattery(sticky)
        status = status.copy(battery = percent, charging = charging)

        val cm = context.getSystemService(ConnectivityManager::class.java)
        val callback = object : ConnectivityManager.NetworkCallback() {
            override fun onCapabilitiesChanged(network: Network, caps: NetworkCapabilities) = refreshNetwork()
            override fun onLost(network: Network) = refreshNetwork()
            override fun onAvailable(network: Network) = refreshNetwork()
        }
        runCatching { cm?.registerDefaultNetworkCallback(callback) }
        refreshNetwork()
        onDispose {
            runCatching { context.unregisterReceiver(receiver) }
            runCatching { cm?.unregisterNetworkCallback(callback) }
        }
    }
    // Starsze Androidy nie zgłaszają zmian RSSI przez callback — dopytujemy co 10 s.
    LaunchedEffect(context) {
        while (true) {
            delay(REFRESH_MS)
            refreshNetwork()
        }
    }
    return status
}

@Composable
fun StatusPill(status: DeviceStatus, modifier: Modifier = Modifier) {
    Row(
        modifier.height(28.u).clip(CircleShape).background(C.surface).border(1.u, C.line, CircleShape)
            .padding(horizontal = 12.u),
        verticalAlignment = Alignment.CenterVertically,
        horizontalArrangement = Arrangement.spacedBy(8.u),
    ) {
        val noWifi = status.wifiLevel == 0
        val wifiColor = when {
            noWifi -> C.danger
            status.wifiLevel == 1 -> C.accent
            else -> C.dim
        }
        WifiIcon(status.wifiLevel, wifiColor)
        if (noWifi) T(if (status.offline) "brak sieci" else "brak Wi‑Fi", 13, FontWeight.Medium, C.danger)
        status.battery?.let { percent ->
            Box(Modifier.width(1.u).height(14.u).background(C.line))
            val color = when {
                status.charging -> C.ok
                percent <= BATTERY_LOW -> C.danger
                else -> C.dim
            }
            BatteryIcon(percent, status.charging, color)
            T("$percent%", 13, FontWeight.Medium, color)
        }
    }
}

/** Trzy łuki nad kropką; przy braku sieci przygaszone i przekreślone. */
@Composable
private fun WifiIcon(level: Int, color: Color) {
    Canvas(Modifier.size(22.u, 16.u)) {
        val unit = size.width / 22f
        val center = Offset(size.width / 2, 14f * unit)
        val stroke = Stroke(width = 2f * unit, cap = StrokeCap.Round)
        listOf(4f, 8f, 12f).forEachIndexed { i, r ->
            val radius = r * unit
            drawArc(
                color = color.copy(alpha = if (level >= i + 1) 1f else 0.3f),
                startAngle = 225f, sweepAngle = 90f, useCenter = false,
                topLeft = Offset(center.x - radius, center.y - radius), size = Size(2 * radius, 2 * radius),
                style = stroke,
            )
        }
        drawCircle(color.copy(alpha = if (level == 0) 0.3f else 1f), radius = 2f * unit, center = center)
        if (level == 0) {
            drawLine(color, Offset(2f * unit, 0f), Offset(size.width - 2f * unit, size.height), 2f * unit, StrokeCap.Round)
        }
    }
}

/** Obrys z końcówką, wypełnienie wg procentu, błyskawica przy ładowaniu. */
@Composable
private fun BatteryIcon(percent: Int, charging: Boolean, color: Color) {
    val bolt = C.surface
    Canvas(Modifier.size(22.u, 12.u)) {
        val unit = size.width / 22f
        drawRoundRect(
            color.copy(alpha = 0.6f), topLeft = Offset(0.5f * unit, 0.5f * unit), size = Size(18f * unit, 11f * unit),
            cornerRadius = CornerRadius(3f * unit), style = Stroke(1.2f * unit),
        )
        drawRoundRect(
            color.copy(alpha = 0.6f), topLeft = Offset(20f * unit, 4f * unit), size = Size(2f * unit, 4f * unit),
            cornerRadius = CornerRadius(1f * unit),
        )
        val fill = (14f * percent.coerceIn(0, 100) / 100f).coerceAtLeast(1.5f)
        drawRoundRect(
            color, topLeft = Offset(2.5f * unit, 2.5f * unit), size = Size(fill * unit, 7f * unit),
            cornerRadius = CornerRadius(1.2f * unit),
        )
        if (charging) {
            val path = Path().apply {
                moveTo(11f * unit, 1.5f * unit); lineTo(7f * unit, 6.5f * unit); lineTo(12f * unit, 6f * unit); lineTo(8f * unit, 10.5f * unit)
            }
            drawPath(path, bolt, style = Stroke(2f * unit, cap = StrokeCap.Round))
        }
    }
}
