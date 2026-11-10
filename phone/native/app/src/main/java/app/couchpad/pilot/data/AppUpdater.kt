package app.couchpad.pilot.data

import android.app.PendingIntent
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.pm.PackageInstaller
import android.net.Uri
import android.os.Build
import android.provider.Settings
import android.util.Log
import androidx.core.content.ContextCompat
import app.couchpad.pilot.BuildConfig
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import kotlinx.coroutines.launch
import org.json.JSONObject
import java.io.File
import java.net.HttpURLConnection
import java.net.URL
import java.security.MessageDigest

/** Wersja opublikowana na brokerze (retained `multimedia_remote/app/android`, ADR 0021). */
data class AppRelease(
    val versionCode: Int,
    val versionName: String,
    val url: String,
    val sha256: String,
    val size: Long,
) {
    val isNewer get() = versionCode > BuildConfig.VERSION_CODE

    companion object {
        /** Niepełny albo uszkodzony manifest — null, pilot nic nie proponuje. */
        fun parse(text: String): AppRelease? = runCatching {
            val o = JSONObject(text)
            AppRelease(
                versionCode = o.getInt("version_code"),
                versionName = o.optString("version_name"),
                url = o.getString("url").takeIf { it.startsWith("http://") || it.startsWith("https://") }!!,
                sha256 = o.getString("sha256").lowercase().takeIf { it.length == 64 }!!,
                size = o.optLong("size", 0),
            )
        }.getOrNull()
    }
}

sealed interface UpdatePhase {
    data object Idle : UpdatePhase
    /** [progress] 0..1 albo null, gdy serwer nie podał rozmiaru. */
    data class Downloading(val progress: Float?) : UpdatePhase
    /** Android pokazuje swoje okno instalacji; po „Zainstaluj” proces pilota zostanie zabity. */
    data object Installing : UpdatePhase
    data class Failed(val message: String) : UpdatePhase
}

/**
 * Aktualizacja aplikacji przez Wi-Fi (ADR 0021): pobranie APK z adresu z manifestu,
 * sprawdzenie sha256 i instalacja przez [PackageInstaller]. Android i tak prosi o
 * potwierdzenie — bez uprawnień właściciela urządzenia nie da się tego ominąć.
 */
class AppUpdater(context: Context) {
    private val app = context.applicationContext
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.IO)
    private var job: Job? = null

    private val _phase = MutableStateFlow<UpdatePhase>(UpdatePhase.Idle)
    val phase: StateFlow<UpdatePhase> = _phase.asStateFlow()

    /** Android 8+: zgoda „instaluj nieznane aplikacje” dla pilota. Na 6–7 decyduje ustawienie globalne. */
    fun canInstall(): Boolean =
        Build.VERSION.SDK_INT < Build.VERSION_CODES.O || app.packageManager.canRequestPackageInstalls()

    /** Ekran systemowy, na którym użytkownik zezwala pilotowi instalować aplikacje. */
    fun permissionIntent(): Intent =
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
            Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES, Uri.parse("package:${app.packageName}"))
        } else {
            Intent(Settings.ACTION_SECURITY_SETTINGS)
        }.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)

    fun install(release: AppRelease) {
        if (job?.isActive == true) return
        job = scope.launch {
            _phase.value = UpdatePhase.Downloading(null)
            val result = runCatching {
                val file = download(release)
                _phase.value = UpdatePhase.Installing
                commit(file)
            }
            result.exceptionOrNull()?.let {
                Log.w(TAG, "aktualizacja nieudana", it)
                _phase.value = UpdatePhase.Failed(it.message ?: it.javaClass.simpleName)
            }
        }
    }

    fun dismissError() {
        if (_phase.value is UpdatePhase.Failed) _phase.value = UpdatePhase.Idle
    }

    private fun download(release: AppRelease): File {
        val dir = File(app.cacheDir, "update").apply { mkdirs() }
        dir.listFiles()?.forEach { it.delete() }
        val file = File(dir, "pilot-${release.versionCode}.apk")
        val conn = URL(release.url).openConnection() as HttpURLConnection
        conn.connectTimeout = 10_000
        conn.readTimeout = 20_000
        try {
            if (conn.responseCode != 200) error("Serwer zwrócił ${conn.responseCode}")
            val total = conn.contentLengthLong.takeIf { it > 0 } ?: release.size.takeIf { it > 0 }
            val digest = MessageDigest.getInstance("SHA-256")
            var done = 0L
            var shown = -1
            conn.inputStream.use { input ->
                file.outputStream().use { out ->
                    val buf = ByteArray(64 * 1024)
                    while (true) {
                        val n = input.read(buf)
                        if (n < 0) break
                        out.write(buf, 0, n)
                        digest.update(buf, 0, n)
                        done += n
                        if (total != null) {
                            val pct = (done * 100 / total).toInt()
                            if (pct != shown) {
                                shown = pct
                                _phase.value = UpdatePhase.Downloading(done.toFloat() / total)
                            }
                        }
                    }
                }
            }
            val hex = digest.digest().joinToString("") { "%02x".format(it) }
            if (hex != release.sha256) {
                file.delete()
                error("Suma kontrolna się nie zgadza")
            }
            return file
        } finally {
            conn.disconnect()
        }
    }

    private fun commit(file: File) {
        val installer = app.packageManager.packageInstaller
        val params = PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL)
            .apply { setAppPackageName(app.packageName) }
        val id = installer.createSession(params)
        installer.openSession(id).use { session ->
            file.inputStream().use { input ->
                session.openWrite("pilot.apk", 0, file.length()).use { out ->
                    input.copyTo(out)
                    session.fsync(out)
                }
            }
            registerResultReceiver()
            val intent = Intent(ACTION_RESULT).setPackage(app.packageName)
            // System dopisuje do intencji status i okno potwierdzenia, więc musi być modyfikowalna.
            val flags = PendingIntent.FLAG_UPDATE_CURRENT or
                (if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) PendingIntent.FLAG_MUTABLE else 0)
            session.commit(PendingIntent.getBroadcast(app, id, intent, flags).intentSender)
        }
        file.delete()
    }

    @Volatile private var receiverRegistered = false

    private fun registerResultReceiver() {
        if (receiverRegistered) return
        receiverRegistered = true
        ContextCompat.registerReceiver(app, object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) = onResult(intent)
        }, IntentFilter(ACTION_RESULT), ContextCompat.RECEIVER_NOT_EXPORTED)
    }

    private fun onResult(intent: Intent) {
        when (val status = intent.getIntExtra(PackageInstaller.EXTRA_STATUS, PackageInstaller.STATUS_FAILURE)) {
            PackageInstaller.STATUS_PENDING_USER_ACTION -> {
                @Suppress("DEPRECATION")
                val confirm = intent.getParcelableExtra<Intent>(Intent.EXTRA_INTENT) ?: return
                app.startActivity(confirm.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
            }
            // Po udanej instalacji proces zwykle już nie żyje; gdyby żył — wracamy do spoczynku.
            PackageInstaller.STATUS_SUCCESS -> _phase.value = UpdatePhase.Idle
            PackageInstaller.STATUS_FAILURE_ABORTED -> _phase.value = UpdatePhase.Idle
            else -> {
                val msg = intent.getStringExtra(PackageInstaller.EXTRA_STATUS_MESSAGE)
                _phase.value = UpdatePhase.Failed(msg ?: "Instalacja nieudana ($status)")
            }
        }
    }

    private companion object {
        const val TAG = "AppUpdater"
        const val ACTION_RESULT = "app.couchpad.pilot.UPDATE_RESULT"
    }
}
