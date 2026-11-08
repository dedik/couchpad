package app.couchpad.pilot.ui.settings

import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.provider.Settings
import androidx.compose.foundation.Image
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.Spacer
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.height
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.width
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.rounded.ChevronRight
import androidx.compose.material3.Icon
import androidx.compose.runtime.Composable
import androidx.compose.runtime.DisposableEffect
import androidx.compose.runtime.State
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.produceState
import androidx.compose.runtime.remember
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.ImageBitmap
import androidx.compose.ui.graphics.asImageBitmap
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.platform.LocalLifecycleOwner
import androidx.compose.ui.text.font.FontWeight
import androidx.core.graphics.drawable.toBitmap
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleEventObserver
import app.couchpad.pilot.ui.components.Button
import app.couchpad.pilot.ui.components.Card
import app.couchpad.pilot.ui.theme.C
import app.couchpad.pilot.ui.theme.T
import app.couchpad.pilot.ui.theme.u
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext

/** Czy pilot jest domyślnym ekranem głównym (launcherem) telefonu. */
private fun isDefaultHome(context: Context): Boolean {
    val home = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME)
    val resolved = context.packageManager.resolveActivity(home, PackageManager.MATCH_DEFAULT_ONLY)
    return resolved?.activityInfo?.packageName == context.packageName
}

/** [isDefaultHome] odświeżane po każdym powrocie na wierzch (np. z systemowego wyboru). */
@Composable
internal fun rememberIsDefaultHome(): State<Boolean> {
    val context = LocalContext.current
    val state = remember { mutableStateOf(isDefaultHome(context)) }
    val lifecycle = LocalLifecycleOwner.current.lifecycle
    DisposableEffect(lifecycle) {
        val observer = LifecycleEventObserver { _, event ->
            if (event == Lifecycle.Event.ON_RESUME) state.value = isDefaultHome(context)
        }
        lifecycle.addObserver(observer)
        onDispose { lifecycle.removeObserver(observer) }
    }
    return state
}

private data class PhoneApp(val label: String, val intent: Intent, val icon: ImageBitmap?)

private fun launchableApps(context: Context): List<PhoneApp> {
    val pm = context.packageManager
    val query = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
    return pm.queryIntentActivities(query, 0)
        .filter { it.activityInfo.packageName != context.packageName }
        .map { info ->
            val intent = Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_LAUNCHER)
                .setClassName(info.activityInfo.packageName, info.activityInfo.name)
                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_RESET_TASK_IF_NEEDED)
            val icon = runCatching { info.loadIcon(pm).toBitmap(96, 96).asImageBitmap() }.getOrNull()
            PhoneApp(info.loadLabel(pm).toString(), intent, icon)
        }
        .sortedBy { it.label.lowercase() }
}

private fun Context.open(action: String) = startActivity(Intent(action).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))

/**
 * Pilot jako launcher zabiera ekran główny (ADR 0010), więc stąd wychodzi się do reszty
 * telefonu: wybór ekranu głównego, ustawienia Androida i lista aplikacji.
 */
@Composable
internal fun PhonePage() {
    val context = LocalContext.current
    val isHome by rememberIsDefaultHome()
    // Ikony i nazwy aplikacji czytamy poza wątkiem UI — przy kilkudziesięciu to zauważalne.
    val apps by produceState(emptyList<PhoneApp>()) { value = withContext(Dispatchers.IO) { launchableApps(context) } }

    InfoRow("Ekran główny", if (isHome) "pilot" else "inny launcher", isHome)
    Note(
        if (isHome) "Pilot startuje razem z telefonem, Home wraca do niego, a po zapaleniu ekranu pokazuje się od razu."
        else "Ustaw pilota jako ekran główny: będzie startował razem z telefonem i pojawiał się po zapaleniu ekranu."
    )
    Button(if (isHome) "Zmień ekran główny" else "Ustaw jako ekran główny", Modifier.fillMaxWidth(), accent = !isHome) {
        runCatching { context.open(Settings.ACTION_HOME_SETTINGS) }.onFailure { context.open(Settings.ACTION_SETTINGS) }
    }
    Button("Ustawienia Androida", Modifier.fillMaxWidth()) { context.open(Settings.ACTION_SETTINGS) }
    Group("Aplikacje") {
        Column(verticalArrangement = Arrangement.spacedBy(8.u)) {
            for (app in apps) {
                Card(Modifier.fillMaxWidth().height(56.u), radius = 14, onClick = { runCatching { context.startActivity(app.intent) } }) {
                    Row(Modifier.fillMaxSize().padding(horizontal = 14.u), verticalAlignment = Alignment.CenterVertically) {
                        if (app.icon != null) {
                            Image(app.icon, null, Modifier.size(32.u))
                            Spacer(Modifier.width(14.u))
                        }
                        T(app.label, 15, FontWeight.Medium, modifier = Modifier.weight(1f))
                        Icon(Icons.Rounded.ChevronRight, null, tint = C.faint, modifier = Modifier.size(20.u))
                    }
                }
            }
        }
    }
}
