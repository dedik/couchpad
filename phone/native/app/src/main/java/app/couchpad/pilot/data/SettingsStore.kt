package app.couchpad.pilot.data

import android.content.Context
import android.content.SharedPreferences
import android.provider.Settings
import org.json.JSONObject

/** Ustawienia pilota w prywatnych SharedPreferences aplikacji (`shared_prefs/pilot.xml`). */
class SettingsStore(context: Context) {
    private val prefs: SharedPreferences = context.getSharedPreferences("pilot", Context.MODE_PRIVATE)

    /**
     * Krótki znacznik tego telefonu (z ANDROID_ID, nie z ustawień — te kopiuje się między
     * telefonami). Broker rozłącza starsze połączenie o tym samym identyfikatorze klienta,
     * więc dwa telefony z jednym pilotem wyrzucałyby się nawzajem.
     */
    @Suppress("HardwareIds")
    val deviceTag: String = runCatching {
        Settings.Secure.getString(context.contentResolver, Settings.Secure.ANDROID_ID)
    }.getOrNull()?.takeLast(6)?.ifBlank { null } ?: "phone"

    fun loadBroker(): BrokerSettings {
        val d = BrokerSettings()
        return BrokerSettings(
            host = string("host", d.host),
            port = prefs.getInt("port", d.port),
            username = string("username", d.username),
            password = string("password", d.password),
            z906Id = string("z906_id", d.z906Id),
            remoteId = string("remote_id", d.remoteId),
            tvPrefix = string("tv_prefix", d.tvPrefix),
        )
    }

    fun saveBroker(s: BrokerSettings) {
        prefs.edit()
            .putString("host", s.host.trim())
            .putInt("port", s.port)
            .putString("username", s.username)
            .putString("password", s.password)
            .putString("z906_id", s.z906Id.trim())
            .putString("remote_id", s.remoteId.trim())
            .putString("tv_prefix", s.tvPrefix.trim())
            .apply()
    }

    fun loadPreferences(): Preferences {
        val d = Preferences()
        return Preferences(
            roomName = string("room", d.roomName),
            orientation = prefs.getInt("orientation", d.orientation),
            theme = prefs.getInt("theme", d.theme),
            themeHours = prefs.getInt("theme_hours", d.themeHours),
            brightness = prefs.getInt("brightness", d.brightness),
            timeout = prefs.getInt("timeout", d.timeout),
            dimBeforeSleep = prefs.getBoolean("dim_before_sleep", d.dimBeforeSleep),
            night = prefs.getInt("night", d.night),
            timeZone = prefs.getInt("time_zone", d.timeZone),
            keyboardLive = prefs.getBoolean("keyboard_live", d.keyboardLive),
            keyboardAutoOpen = prefs.getBoolean("keyboard_auto", d.keyboardAutoOpen),
            trackpadStep = prefs.getInt("trackpad_step", d.trackpadStep),
            volumeLimit = prefs.getInt("volume_limit", d.volumeLimit),
            oled = prefs.getBoolean("oled", d.oled),
        )
    }

    fun savePreferences(p: Preferences) {
        prefs.edit()
            .putString("room", p.roomName)
            .putInt("orientation", p.orientation)
            .putInt("theme", p.theme)
            .putInt("theme_hours", p.themeHours)
            .putInt("brightness", p.brightness)
            .putInt("timeout", p.timeout)
            .putBoolean("dim_before_sleep", p.dimBeforeSleep)
            .putInt("night", p.night)
            .putInt("time_zone", p.timeZone)
            .putBoolean("keyboard_live", p.keyboardLive)
            .putBoolean("keyboard_auto", p.keyboardAutoOpen)
            .putInt("trackpad_step", p.trackpadStep)
            .putInt("volume_limit", p.volumeLimit)
            .putBoolean("oled", p.oled)
            .apply()
    }

    /** Liczniki wyborów efektów: lampa → efekt → ile razy wybrany (`effect_usage`, JSON). */
    fun loadEffectUsage(): Map<String, Map<String, Int>> {
        val root = runCatching { JSONObject(string("effect_usage", "{}")) }.getOrNull() ?: return emptyMap()
        return root.keys().asSequence().associateWith { light ->
            val counts = root.optJSONObject(light) ?: JSONObject()
            counts.keys().asSequence().associateWith { counts.optInt(it) }
        }
    }

    fun saveEffectUsage(usage: Map<String, Map<String, Int>>) {
        val root = JSONObject()
        usage.forEach { (light, counts) -> root.put(light, JSONObject(counts)) }
        prefs.edit().putString("effect_usage", root.toString()).apply()
    }

    /** Ustawienia fabryczne: broker, identyfikatory i wszystkie preferencje. */
    fun clearAll() {
        prefs.edit().clear().apply()
    }

    private fun string(key: String, default: String) = prefs.getString(key, default) ?: default
}
