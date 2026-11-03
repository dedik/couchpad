#pragma once

// Zegar z sieci. Pilot nie ma podtrzymywanego zegara, więc godzina na pasku
// statusu bierze się z SNTP i pojawia dopiero po połączeniu z Wi-Fi.
//
// Synchronizacja jest **jednorazowa**: pobieramy czas, zatrzymujemy klienta
// i więcej nie zaglądamy do sieci. Przy pracy na baterii stałe odpytywanie
// wybudzałoby pilota bez powodu.
//
// Jedyny wyjątek to doba bez korekty — oscylator układu dryfuje i po kilku dniach
// godzina uciekłaby o minuty.

namespace transport {

// Zleca pobranie czasu, o ile jest potrzebne. Wołać po uzyskaniu połączenia.
void start_time_sync();

// Wołane cyklicznie z zadania UI: zatrzymuje klienta, gdy czas już przyszedł
// albo gdy nie przyszedł w rozsądnym czasie.
void service_time_sync();

// `false`, dopóki nie przyszedł pierwszy czas — interfejs pokazuje wtedy kreski
// zamiast zmyślonej godziny.
bool time_known();
// Strefa czasowa w zapisie POSIX (np. "CET-1CEST,M3.5.0,M10.5.0/3"). Bez wywołania
// obowiązuje czas polski.
void set_timezone(const char* posix);

}  // namespace transport
