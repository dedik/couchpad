# Kontrakt MQTT v1

[English](mqtt-contract.en.md)

## Zasada

Home Assistant jest właścicielem rzeczywistych urządzeń i stanów. Pilot nie publikuje na istniejące, wewnętrzne tematy Z906 i nie zakłada nazw encji. Komunikuje się przez dedykowany kontrakt pilota, który obsługuje automatyzacja lub bridge po stronie Home Assistant.

Tematy są wersjonowane, aby przyszła zmiana nie złamała używanych pilotów.

```text
multimedia_remote/v1/<remote_id>/command       pilot → Home Assistant
multimedia_remote/v1/<remote_id>/state         Home Assistant → pilot
multimedia_remote/v1/<remote_id>/availability  obie strony
```

`<remote_id>` jest stabilnym identyfikatorem zapisanym lokalnie podczas pierwszej konfiguracji. W przykładach używamy `living-room-remote`; nie jest to domyślna, narzucona nazwa.

## Polecenie

Temat `command`: QoS 1, bez retain. Wyjątek: `appletv.navigate` idzie z QoS 0 — to chwilowy klawisz, a ponowiony po czasie poruszyłby Apple TV bez udziału użytkownika.

```json
{
  "id": "3f5575f7-1b5f-4f31-8129-82b2e5b2fca2",
  "action": "z906.volume.set",
  "args": { "percent": 42 }
}
```

Minimalne akcje pierwszego wydania:

```text
tv.power.set          { "on": true }
tv.navigate            { "key": "up|down|left|right|ok|back|home" }
tv.media               { "key": "play_pause|stop|rewind|fast_forward" }
tv.source.set         { "source": "exact option exposed by TV" }
tv.picture_mode.set    { "mode": "exact option exposed by TV" }
z906.power.set         { "on": true }
z906.mute.set          { "muted": true }
z906.volume.set        { "percent": 0..100 }
z906.level.set         { "channel": "center|subwoofer|rear", "percent": 0..100 }
z906.source.set        { "source": "..." }
z906.effect.set        { "effect": "..." }
ha.action              { "id": "named-action" }
remote.sync.request    {}
```

Akcje Apple TV ([ADR 0007](adr/0007-apple-tv-via-home-assistant.md)):

```text
appletv.power.set       { "on": true }
appletv.navigate        { "key": "up|down|left|right|select|menu|home", "action": "tap|double|hold" }
appletv.app.launch      { "app_id": "bundle id z appletv.apps[].id" }
appletv.keyboard.set    { "text": "..." }      zastępuje cały tekst aktywnego pola
appletv.keyboard.clear  {}
```

Akcje klimatyzacji ([ADR 0009](adr/0009-climate-via-home-assistant.md)):

```text
climate.power.set             { "on": true }              włączenie wraca do ostatniego trybu
climate.mode.set              { "mode": "cool" }          identyfikator z climate.hvac_modes
climate.target.set            { "temperature": 22.5 }     w zakresie i z krokiem z encji
climate.fan.set               { "mode": "medlow" }        z climate.fan_modes
climate.swing.set             { "mode": "Full" }          z climate.swing_modes
climate.swing_horizontal.set  { "mode": "Left" }          z climate.swing_horizontal_modes
```

`action` w `appletv.navigate` jest opcjonalne, domyślnie `tap`. `double` i `hold` odtwarzają
ukryte gesty pilota Siri: Home ×2 otwiera przełącznik aplikacji, przytrzymanie Home — Centrum
sterowania, przytrzymanie `select` — menu kontekstowe.

`appletv.navigate` jest zdarzeniem chwilowym: bridge wykonuje je bez publikowania snapshotu,
bo przesunięcie palcem po trackpadzie daje kilka kroków na sekundę. `appletv.app.launch`
przyjmuje wyłącznie bundle id z listy `apps` w ostatnim snapshocie; inne bridge odrzuca.

Polecenia ustawiają stan jawnie; nie wprowadzamy `toggle`, jeśli UI nie ma świeżego, potwierdzonego stanu. Dla suwaka głośności klient wysyła maksymalnie 10 poleceń/s i zawsze wysyła końcową wartość po puszczeniu palca.

## Snapshot stanu

Temat `state`: QoS 1, retained. Każdy komunikat zawiera pełny, samowystarczalny obraz stanu oraz rosnącą rewizję.

```json
{
  "revision": 41,
  "z906": {
    "available": true,
    "power": "on",
    "muted": false,
    "volume_percent": 42,
    "source": "input_2",
    "source_options": ["PC - analog 5.1", "RCA - stereo", "Optical 1", "Optical 2", "Coaxial", "AUX"],
    "center_percent": 56,
    "subwoofer_percent": 49,
    "rear_percent": 68,
    "effect": "stereo",
    "effect_options": ["Stereo 3D", "Stereo 2.1", "Stereo 4.1", "No effect"],
    "decode_5_1": false
  },
  "tv": {
    "available": true,
    "power": "on",
    "label": "LG OLED",
    "source": "HDMI 1",
    "source_options": ["HDMI 1", "HDMI 2", "TV", "Aplikacje"],
    "picture_mode": "Filmmaker Mode",
    "picture_mode_options": ["Vivid", "Standard", "Cinema", "Filmmaker Mode"]
  },
  "appletv": {
    "available": true,
    "power": "on",
    "app_id": "<bundle id aktywnej aplikacji>",
    "apps": [
      { "id": "<bundle id>", "label": "<nazwa z source_list>" }
    ],
    "keyboard_active": false
  },
  "climate": {
    "available": true,
    "hvac_mode": "cool",
    "hvac_modes": ["off", "heat_cool", "cool", "heat", "fan_only", "dry"],
    "target": 22.0,
    "current": 24.0,
    "min": 16.0,
    "max": 31.0,
    "step": 0.5,
    "fan_mode": "medium",
    "fan_modes": ["auto", "quiet", "low", "medlow", "medium", "medhigh", "high", "strong"],
    "swing_mode": "Full",
    "swing_modes": ["off", "Full", "Upper", "Lower"],
    "swing_horizontal_mode": "Full",
    "swing_horizontal_modes": ["off", "Full", "Left", "Center", "Right"]
  },
  "shortcuts": [
    { "id": "movie-mode", "label": "Film", "enabled": true }
  ]
}
```

Sekcja `appletv`:

- `apps` to lista ulubionych aplikacji ustawiona w bridge'u (najwyżej 8), a nie pełna
  `source_list`. Bridge publikuje tylko te, które Apple TV faktycznie ma zainstalowane.
- Kluczem aplikacji jest bundle id. `label` służy wyłącznie do wyświetlania, bo nazwy
  w `source_list` zależą od wersji aplikacji i języka systemu.
- `app_id` jest pusty, gdy Apple TV stoi na ekranie głównym albo śpi.
- `keyboard_active` mówi, że na Apple TV jest aktywne pole tekstowe; pilot wysuwa wtedy
  klawiaturę.

Sekcja `climate`:

- Tryby i opcje to identyfikatory HA, nie nazwy do pokazania — polskie nazwy dobiera pilot.
  Każda lista ma najwyżej 8 pozycji; pusta lista albo brak pola chowa dane sterowanie.
- `hvac_mode` równe `off` oznacza wyłączoną klimę. W `fan_only` temperatura docelowa nie działa.
- `current` jest `null`, gdy czujnik w pokoju nie podaje wartości.
- Temperatury w stopniach; pilot zaokrągla je do dziesiątych.

Pilot ignoruje snapshot o rewizji nie większej niż ostatnio przetworzona. Przy braku stanu pokazuje urządzenie jako niedostępne, nie zgaduje jego statusu.

Pola `source` i `effect` niosą **bieżącą opcję z odpowiadającej im listy**, a nie wewnętrzny
identyfikator wejścia. Pilot dopasowuje wartość do `source_options` i `effect_options`, żeby
zaznaczyć właściwy segment; wartość spoza listy nie zaznaczy niczego. Przykład wyżej pokazuje
`input_2` dla czytelności struktury, ale poprawny payload powtarza tam jedną z opcji.

`tv.picture_mode` jest bieżącym stanem raportowanym przez telewizor i może zmienić się bez akcji użytkownika, np. zależnie od materiału. UI aktualizuje go po każdym snapshocie. Przycisk zmiany trybu jest dostępny tylko wtedy, gdy `picture_mode_options` zawiera przynajmniej jedną opcję; nazwy opcji nie są na stałe zaszyte w firmware.

`tv.source` i `tv.source_options` działają analogicznie: UI pokazuje wyłącznie opcje przekazane w świeżym snapshocie.

**Telewizor korzysta z tego kontraktu tylko dla kanałów.** Od [ADR 0008](adr/0008-tv-via-glasshouse.md) stan, nawigacja, źródło i tryb obrazu idą wprost na tematy Glasshouse `<prefiks>/...`. Kontraktem idzie wyłącznie skrót kanału, bo Glasshouse przełącza kanał tylko po HTTP:

```text
tv.channel.open        { "name": "Polsat" }               skrót: nazwa z listy tunera, bez wielkości liter i spacji
tv.channel.open        { "name": "...", "id": "<id>" }    menu kanałów: identyfikator z listy `channels` ma pierwszeństwo
tv.channels.request    {}                                 prośba o ponowne opublikowanie listy kanałów
```

Bridge wykonuje je bez publikowania snapshotu.

Listę kanałów bridge publikuje na osobnym temacie, QoS 1, retained — po starcie HA, co godzinę i po `tv.channels.request` (pilot wysyła je po każdym połączeniu):

```text
multimedia_remote/v1/<remote_id>/channels      Home Assistant → pilot
```

```json
{ "channels": [ { "number": "4", "name": "Polsat", "id": "1_23_4_4_2_3_8808" } ] }
```

Kolejność jest kolejnością tunera, bez stacji radiowych. Lista nie należy do snapshotu: ma kilka kilobajtów i zmienia się tylko po wyszukaniu kanałów. Pilot pokazuje wyłącznie listę odebraną z tego tematu i przełącza po `id`, bo nazwy bywają zdublowane. Program +/- nie idzie tym kontraktem: to klawisz pilota wysyłany wprost do Glasshouse (`<prefiks>/command/rcu channelup|channeldown`). Pozostałe akcje `tv.*` i sekcja `tv` snapshotu zostają opisane wyżej dla zgodności, ale bridge ich nie obsługuje, a firmware ich nie wysyła.

Pogodę ([ADR 0011](adr/0011-weather-on-start-screen.md)) bridge publikuje na osobnym temacie, QoS 1, retained — przy zmianie warunku lub temperatury i po starcie HA:

```text
multimedia_remote/v1/<remote_id>/weather       Home Assistant → pilot
```

```json
{ "condition": "partlycloudy", "temperature": 5.5, "night": true }
```

`condition` to identyfikator stanu encji `weather` w HA; pilot dobiera do niego ikonę i polską nazwę. `night` mówi, że słońce jest pod horyzontem (`sun.sun`): HA ma nocny wariant tylko dla bezchmurnego nieba (`clear-night`), więc np. `partlycloudy` w nocy pilot rysuje z księżycem. Pogoda nie należy do snapshotu: to treść, nie stan urządzenia, więc pilot pokazuje ostatnią z pamięci od razu po wybudzeniu, a po 6 godzinach bez aktualizacji ją chowa.

Automatyzacja/bridge Home Assistanta publikuje snapshot po każdej zmianie TV, Z906, Apple TV, klimatyzacji lub skrótu, po restarcie oraz po otrzymaniu `remote.sync.request`. Żądanie synchronizacji po wybudzeniu musi wygenerować nową rewizję po sprawdzeniu bieżącego stanu przez Home Assistant; nie może jedynie odesłać starej wiadomości retained.

## Dostępność i błędy

- Pilot publikuje `online` oraz LWT `offline` na swoim temacie availability, QoS 1 i retain.
- Wysłane polecenie dostaje lokalny stan „oczekuje”; znika on po nowym snapshocie, odrzuceniu połączenia albo przekroczeniu limitu czasu.
- Błąd transportu nie może blokować UI ani powodować wielokrotnych, niekontrolowanych prób wykonania akcji.
- Ekran sterowania po wybudzeniu jest dostępny wyłącznie po otrzymaniu nowego snapshotu wygenerowanego w odpowiedzi na `remote.sync.request`. Przed nim pokazywany jest ekran łączenia, a nie stan z pamięci.

Zmiana tematu, payloadu lub semantyki wymaga nowej wersji (`v2`) albo pełnej zgodności wstecznej i ADR.
