# ADR 0004: Connected standby jako domyślny tryb bezczynności

- **Status:** zastąpione przez ADR 0005
- **Data:** 2026-10-29

## Wymaganie historyczne

Dotknięcie wygaszonego pilota miało od razu pokazywać ostatni potwierdzony stan TV oraz Z906.

## Historyczna decyzja

Po okresie bezczynności wygaszamy wyłącznie ekran i podświetlenie. ESP32-P4, ESP32-C6, Wi-Fi, MQTT oraz lokalny `AppState` nadal działają. Pilot subskrybuje pełny snapshot Home Assistanta i otrzymuje jego zmiany również przy wygaszonym ekranie.

Przy wybudzeniu UI renderuje dane znajdujące się już w RAM, po czym równolegle wysyła `remote.sync.request`. Brak MQTT lub stary snapshot są jawnie sygnalizowane.

## Dlaczego została zastąpiona

Użytkownik zaakceptował około 2–3 s opóźnienia po dotyku, o ile pilot pokaże nowy stan. Powinno to ograniczyć pobór energii na baterii i wyeliminować ryzyko prezentowania poprzedniego stanu jako bieżącego.

- Reakcja po dotyku nie zależy od opóźnienia TV, Z906, Wi-Fi ani Home Assistanta.
- Zużycie energii w bezczynności będzie większe niż w głębokim śnie; zmierzymy je później przed ostatecznym wyborem parametrów baterii.
- Głęboki sen może istnieć jako tryb ręczny lub krytycznie niskiej baterii, ale po nim pierwsze pokazanie stanu wymaga ponownego połączenia i nie może udawać aktualnego.
