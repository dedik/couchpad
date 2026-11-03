# ADR 0005: Nowy stan po wybudzeniu

[English](0005-fresh-state-after-wake.en.md)

- **Status:** zaakceptowane
- **Data:** 2026-10-29
- **Zastępuje:** ADR 0004

## Wymaganie

Po dotknięciu wybudzanego pilota ekran sterowania może pojawić się około 2–3 s później, ale ma przedstawiać nowy, potwierdzony stan TV i Z906.

## Decyzja

Domyślnie urządzenie przechodzi w tryb oszczędzania energii. Dotyk uruchamia sekwencję Wi-Fi → MQTT → `remote.sync.request`. Home Assistant publikuje po tym żądaniu nowy, pełny snapshot. Dopiero jego odebranie odblokowuje ekran sterowania.

W czasie wybudzania ekran pokazuje celowy widok łączenia, a nie stare kafelki urządzeń. Jeśli synchronizacja zawiedzie, pokazuje błąd połączenia i możliwość ponowienia.

## Jak to wyszło

Zrobione dla dźwięku. Dotyk budzi podświetlenie i od razu wysyła `IntentKind::RequestSync`;
stan wraca na fazę łączenia, a zdejmuje ją dopiero snapshot o nowej rewizji albo limit 15 s.

Kontroler Z906 nie zna żądania synchronizacji, ale publikuje stan jako retained, a w MQTT 3.1.1
każdy SUBSCRIBE dostaje retained na nowo — ponowna subskrypcja jest więc tym żądaniem
([ADR 0006](0006-direct-z906-topics.md)).

Pełne oszczędzanie energii zostaje na później: dziś gaśnie samo podświetlenie, Wi-Fi i MQTT
stoją. Usypianie radia wydłużyłoby wybudzanie z ułamka sekundy do około ośmiu i wymaga
potwierdzenia sprzętowej ścieżki wybudzenia dotykiem.

## Konsekwencje

- Stan widoczny użytkownikowi nie zależy od danych sprzed snu.
- Domyślny pobór energii będzie niższy niż w connected standby.
- Osiągnięcie czasu 2–3 s zależy od siły Wi-Fi, MQTT i integracji Home Assistant; testy ustalą rzeczywisty cel oraz zachowanie przy przekroczeniu limitu.
- Trzeba potwierdzić sprzętową ścieżkę wybudzenia dotykiem dla wybranego trybu zasilania.
