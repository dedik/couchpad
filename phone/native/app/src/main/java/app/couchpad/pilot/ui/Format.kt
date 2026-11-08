package app.couchpad.pilot.ui

import java.text.SimpleDateFormat
import java.util.Calendar
import java.util.Date
import java.util.Locale
import java.util.TimeZone

/** Polskie formaty: przecinek dziesiętny, nazwy dni i miesięcy, wersaliki z ogonkami. */
val Polish: Locale = Locale("pl", "PL")

/** Jedno miejsce po przecinku: „21,5”. */
fun oneDecimal(v: Double): String = String.format(Polish, "%.1f", v)

/** Liczba całkowita z zaokrągleniem: „7”. */
fun noDecimals(v: Double): String = String.format(Polish, "%.0f", v)

fun upperPl(s: String): String = s.uppercase(Polish)

/** Data i godzina w strefie z ustawień (Pokój i czas). */
fun formatIn(pattern: String, zoneId: String, date: Date): String =
    SimpleDateFormat(pattern, Polish).apply { timeZone = TimeZone.getTimeZone(zoneId) }.format(date)

fun hourIn(zoneId: String, now: Long): Int =
    Calendar.getInstance(TimeZone.getTimeZone(zoneId)).apply { timeInMillis = now }.get(Calendar.HOUR_OF_DAY)
