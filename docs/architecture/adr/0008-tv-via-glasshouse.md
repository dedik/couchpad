# ADR 0008 — Telewizor wprost przez Glasshouse

- **Data:** 2026-11-02
- **Status:** przyjęta
- **Zmienia:** [ADR 0002](0002-home-assistant-boundary.md) w zakresie telewizora,
  domyka zapowiedź z [ADR 0006](0006-direct-z906-topics.md), że TV wróci przez Home Assistanta

## Problem

ADR 0006 zostawił telewizor bez źródła: LG nie miał własnego kontrolera MQTT, więc jedyną
drogą wydawał się bridge w Home Assistancie. Od tego czasu na telewizorze działa
[Glasshouse](https://github.com/rorygallagher2024/glasshouse) — serwer uruchomiony na samym
webOS, który publikuje stan i przyjmuje polecenia po MQTT, na tym samym brokerze co Z906.
Home Assistant widzi go przez MQTT discovery (encje `*_<nazwa_tv>_*`).

Bridge w HA dla telewizora oznaczałby tę samą drogę, którą ADR 0006 odrzucił dla dźwięku:

```text
pilot → broker → Home Assistant → broker → Glasshouse
```

oraz zmiany w pakiecie HA i jego restart przy każdej nowej akcji.

## Decyzja

Pilot mówi wyłącznie po MQTT, jak z Z906 i Apple TV. Stan i przyciski idą wprost
do Glasshouse; skróty kanałów — kontraktem v1 do bridge'a w Home Assistancie, bo Glasshouse
przełącza kanał tylko po HTTP:

```text
pilot → broker → Glasshouse                          (stan, nawigacja, źródło, tryb obrazu)
pilot → broker → Home Assistant → HTTP → Glasshouse  (skróty kanałów: tv.channel.open)
```

Home Assistant niczego nie traci: widzi te same tematy i te same encje. Pilot nie zna adresu
telewizora ani tokenu Glasshouse — zna je tylko HA (`secrets.yaml`).

## Tematy

Prefiks ustawia się w Glasshouse (zakładka MQTT), domyślnie `lgtv`. Pilot trzyma go w NVS
razem z danymi brokera.

| Temat | Kierunek | Wartość |
| --- | --- | --- |
| `<p>/status` | TV → | `online`, `off` (czuwanie, serwer działa), `asleep`, `offline` |
| `<p>/telemetry` | TV → | JSON ~5 kB co 10 s, retained: `inputs`, `picture.modes`, `picture.mode_raw`, `app`, `input_source`, `powerState` |
| `<p>/state/application/app` | TV → | krótki identyfikator aplikacji na wierzchu: `hdmi4`, `livetv`, `netflix`… |
| `<p>/state/picture/pictureMode` | TV → | identyfikator trybu obrazu: `standard`, `dolbyHdrCinema`… |
| `<p>/state/power/systemOn` | TV → | `true` / `false` |
| `<p>/command/rcu` | → TV | `up`, `down`, `left`, `right`, `ok`, `back`, `home`, `settings`, `channelup`, `channeldown` |
| `<p>/command/input` | → TV | `livetv`, `hdmi1`–`hdmi4` |
| `<p>/command/picture_mode` | → TV | identyfikator z `picture.modes[].value` |

Tematy `state/...` przychodzą od razu po zmianie; telemetria tylko co 10 s, więc bez nich
potwierdzenie wyboru źródła nie zmieściłoby się w limicie oczekiwania (3 s). Z telemetrii
bierzemy listy opcji i wartości zastępcze, dopóki temat natychmiastowy nie przyszedł.

Nazwy i identyfikatory to dwie różne rzeczy: na ekranie widać „Apple TV” albo „Dolby Vision
Cinema”, polecenie przyjmuje `hdmi4` albo `dolbyHdrCinema`. `TvState` niesie więc obok list
opcji równoległe listy identyfikatorów (`source_ids`, `picture_mode_ids`), a zamiar UI —
identyfikator w `payload`. Firmware nie zaszywa ani jednych, ani drugich.

Tryby obrazu zależą od materiału (SDR, HDR, Dolby Vision). Pilot pokazuje wyłącznie te,
które telewizor właśnie przyjmie.

## Program +/-

P+/P− to klawisze pilota LG `KEY_CHANNELUP` (402) i `KEY_CHANNELDOWN` (403). Usługa
`networkinput/test/sendKeyCode`, przez którą Glasshouse wysyła klawisze, przyjmuje je —
sprawdzone 2026-11-02 `luna-send` na TV, kanał przeskoczył z 6 na 7. Glasshouse 0.138.0 ma
jednak zamkniętą listę klawiszy `RCU_KEYS` bez nich, więc wymaga dopisania `channelup` i
`channeldown` (lokalna łatka na TV i zgłoszenie do autora). Do tego czasu P+/P− na pilocie
kończy się odmową `unknown key` po stronie Glasshouse i niczego nie zmienia.

## Kanały przez Home Assistanta

Glasshouse nie wystawia po MQTT ani przycisków kanałów, ani cyfr. Ma za to HTTP API swojego
widoku Home:

- `GET /api/home/channels` — lista tunera `{id, number, name, radio}`;
- `POST /api/home/open` z `{"kind":"ch","id":"<id>"}` — `tv/openChannel`: stroi kanał
  i wyciąga Live TV na wierzch, bez osobnej zmiany źródła.

Pilot wysyła `tv.channel.open {"name": "Polsat"}` na temat `command` kontraktu v1. Bridge
pobiera listę tunera, dopasowuje nazwę bez wielkości liter i spacji (pierwszy pasujący,
z pominięciem radia) i woła `open`. Lista jest pobierana przy każdym skrócie, bo identyfikator
zawiera parametry multipleksu i zmienia się po ponownym wyszukaniu kanałów.

Zapytania wymagają nagłówka `Authorization: Bearer <token>` (token z `config.json`
Glasshouse) i, dla POST, `Content-Type: application/json`. Robi je `rest_command` w pakiecie
bridge'a; adresy i token są w `secrets.yaml` Home Assistanta.

Polecenie jest chwilowe i nie publikuje snapshotu: stan TV pilot czyta wprost z Glasshouse.

### Menu kanałów

Pełną listę bridge publikuje na retained temacie `multimedia_remote/v1/<id>/channels`
(numer, nazwa, identyfikator; bez radia) po starcie, co godzinę i na `tv.channels.request`,
które pilot wysyła po połączeniu. Wybór z menu wysyła `tv.channel.open` z `id` — nazwy się
powtarzają (TVP World pod 33 i 91), więc bridge nie szuka po nazwie.

Lista żyje poza `DeviceSnapshot` (`domain::ChannelList`, do 96 kanałów, ok. 7 kB): jedna kopia
w kliencie HA pod muteksem, jedna w źródle snapshotów, z której czyta UI. Rozważony był
odczyt na żądanie po HTTP z HA — odrzucony: wymagałby tokenu HA na pilocie i drugiego kanału,
a lista i tak musi leżeć w RAM, gdy menu jest otwarte.

## Co to kosztuje

- **Pilot zna protokół Glasshouse**, tak jak zna protokół kontrolera Z906. Zmiana tematów
  w nowej wersji Glasshouse wymaga zmiany firmware'u. Sprawdzone na wersji 0.138.0.
- **HTTP API kanałów jest wewnętrznym API widoku Home**, nieopisanym w dokumentacji MQTT.
  Może zmienić się bez zapowiedzi.
- **Trzecie połączenie MQTT** (`pilot-tv-<prefiks>`), obok Z906 i Home Assistanta. Na tym
  samym koncie pilota: broker nie ma ACL, więc osobny użytkownik nie jest potrzebny. Konto
  `lgtv` należy do telewizora i pilot go nie używa.
- **Kanały zależą od Home Assistanta.** Bez HA skróty kanałów nie działają; reszta ekranu TV tak.
- **Zasilanie zostaje bez działania.** Włączenie z czuwania wymaga Wake-on-LAN, a telewizor
  ma go wyłączonego (`wakeOnLan: false` w telemetrii). `button.power_on` Glasshouse działa
  tylko z Active Standby. Do osobnej decyzji.

## Co zostaje bez zmian

- Fazę łącza wyznacza Z906 (ADR 0006). Brak Glasshouse wygasza wyłącznie ekran TV.
  Utrata brokera przez klienta TV wygasza ekran dopiero po 5 s: przy słabym Wi-Fi klient
  gubi połączenie co kilka minut i wraca po około 3 s, a broker odsyła wtedy retained stan.
  Polecenia z tej przerwy przepadają i kończą się brakiem potwierdzenia, nie cichym ponowieniem.
- Zasada świeżego stanu z [ADR 0005](0005-fresh-state-after-wake.md): po wybudzeniu pilot
  subskrybuje tematy na nowo, a broker odsyła retained; stan TV do tego czasu jest niedostępny.
- Kontrakt `multimedia_remote/v1` dostaje addytywnie akcję `tv.channel.open`; sekcja `tv`
  snapshotu i pozostałe akcje `tv.*` nie są używane.
