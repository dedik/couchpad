# Bridge po stronie Home Assistanta

[English](README.en.md)

Pilot rozmawia wyłącznie wersjonowanym kontraktem `multimedia_remote/v1` i nie zna encji Z906
ani wewnętrznych tematów jego kontrolera. Tłumaczeniem zajmuje się pakiet
[`multimedia_remote.yaml`](multimedia_remote.yaml). Powód tego podziału opisuje
[ADR 0002](../docs/architecture/adr/0002-home-assistant-boundary.md), a format wiadomości —
[kontrakt MQTT](../docs/architecture/mqtt-contract.md).

Połączenie sieciowe jest bezpośrednie: pilot łączy się z brokerem MQTT po TCP. Home Assistant
nie stoi na ścieżce sieciowej — dokłada wyłącznie treść, bo to on zna stan urządzeń.

**Broker nie musi stać na tej samej maszynie co Home Assistant.** Obaj są jego klientami
i spotykają się na tematach, nie na adresie. W pilocie wpisuje się adres **brokera**, a nie
Home Assistanta; Home Assistant trafia tam przez swoją integrację MQTT. Jedyny warunek to
żeby ta integracja wskazywała na ten sam broker — sprawdzisz to w Ustawienia → Urządzenia
i usługi → MQTT.

## Co robi pakiet

| Element | Rola |
| --- | --- |
| `counter.multimedia_remote_revision` | rosnąca rewizja snapshotu; pilot ignoruje starsze |
| `script.multimedia_remote_publish` | buduje pełny snapshot i publikuje go jako retained |
| automatyzacja „po zmianie Z906, Apple TV lub klimy” | nowy snapshot po każdej zmianie encji; klima — stan i atrybuty pokazywane na pilocie |
| automatyzacja „po starcie” | snapshot po restarcie Home Assistanta |
| automatyzacja „wykonaj polecenie” | odbiera temat `command` i ustawia encje, także `climate.*` ([ADR 0009](../docs/architecture/adr/0009-climate-via-home-assistant.md)) |
| tabela `light_rooms` i automatyzacja „światło” | główne światło pomieszczeń: sekcja `lights` w snapshocie i polecenia `light.*` ([ADR 0014](../docs/architecture/adr/0014-rooms-and-lights.md)); nowe pomieszczenie = wpis w tabeli + encje w wyzwalaczach automatyzacji niżej |
| automatyzacja „snapshot po zmianie światła” | jeden snapshot 0,5 s po zmianach grupy i żarówek (`mode: restart`) |
| tabela `radiator_rooms` i automatyzacja „grzejnik” | głowice termostatyczne: sekcja `radiators` w snapshocie (tryb, nastawa, zawór, bateria, kontaktron okna, następny punkt harmonogramu) i polecenia `radiator.*` ([ADR 0018](../docs/architecture/adr/0018-radiators.md)) |
| automatyzacja „snapshot po zmianie grzejnika” | jeden snapshot 1 s po zmianie trybu, nastawy, `hvac_action` albo baterii (`mode: restart`) |
| automatyzacja „nawigacja Apple TV” | `appletv.navigate` → `remote.send_command`, bez snapshotu |
| skrypt i automatyzacja „lista kanałów TV” | retained lista tunera na `.../channels`: po starcie, co godzinę, na `tv.channels.request` |
| automatyzacja „kanał TV” + `rest_command` | `tv.channel.open` → lista tunera i `open` na Glasshouse, bez snapshotu ([ADR 0008](../docs/architecture/adr/0008-tv-via-glasshouse.md)) |

## Instalacja

1. Włącz pakiety w `configuration.yaml`, jeśli jeszcze ich nie masz:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

2. Skopiuj `multimedia_remote.yaml` do katalogu `packages/` w konfiguracji Home Assistanta.

3. **Podmień `living-room-remote`** na identyfikator wpisany w pilocie
   (`WIĘCEJ → MQTT → Identyfikator pilota`). Występuje w trzech tematach i musi być
   identyczny po obu stronach, bo tworzy temat.

3a. **Apple TV** ([ADR 0007](../docs/architecture/adr/0007-apple-tv-via-home-assistant.md)):
   encje, `atv_config_entry` i `atv_favourites` są już wpisane według
   [`docs/integrations/appletv.md`](../docs/integrations/appletv.md). Po ponownym parowaniu
   integracji zmienia się identyfikator wpisu — trzeba go wtedy podmienić.

3c. **Klimatyzacja** ([ADR 0009](../docs/architecture/adr/0009-climate-via-home-assistant.md)):
   encja w kotwicy `&climate` (`climate_entity: climate.living_room_ac`) i w wyzwalaczach automatyzacji
   „po zmianie…”. Tryby, zakres, krok i opcje bridge bierze z atrybutów encji. Po zmianie
   pakietu wystarczy `check_config` i przeładowanie skryptów oraz automatyzacji.

3b. **Kanały TV** ([ADR 0008](../docs/architecture/adr/0008-tv-via-glasshouse.md)): dopisz
   do `secrets.yaml` adres telewizora i token Glasshouse (z jego `config.json`):

   ```yaml
   glasshouse_channels_url: http://<adres-tv>:8080/api/home/channels
   glasshouse_open_url: http://<adres-tv>:8080/api/home/open
   glasshouse_authorization: Bearer <token>
   ```

   Bez nich pakiet się nie załaduje. `rest_command` przeładowuje się osobno.

3d. **Twoje encje.** Pakiet używa przykładowych nazw. Podmień je na swoje (Ustawienia →
   Urządzenia i usługi → Encje):

   | W pakiecie | Co to jest |
   | --- | --- |
   | `switch.z906_power`, `switch.z906_mute` | zasilanie i wyciszenie Z906 |
   | `number.z906_volume`, `number.z906_center`, `number.z906_subwoofer`, `number.z906_rear` | głośność i poziomy kanałów |
   | `select.z906_input`, `select.z906_effect` | wejście i efekt |
   | `binary_sensor.z906_decode_5_1` | wskaźnik Decode 5.1 |
   | `media_player.living_room_apple_tv`, `remote.living_room_apple_tv`, `binary_sensor.living_room_apple_tv_keyboard_focus` | Apple TV (kotwica `&appletv`) |
   | `WPISZ_ID_WPISU_APPLE_TV` | identyfikator wpisu integracji Apple TV |
   | `climate.living_room_ac` | klimatyzacja (kotwica `&climate`) |
   | `weather.forecast_home` | pogoda na ekranie startowym |

   Encje Z906 występują w kilku szablonach — najprościej podmienić je wyszukiwaniem
   w całym pliku, np.:

   ```bash
   sed -i 's/number\.z906_volume/number.moj_z906_glosnosc/g' multimedia_remote.yaml
   ```

   Jeśli pakiet działa już w Twoim HA, trzymaj swoją kopię poza repozytorium albo przenoś
   zmiany ręcznie — nadpisanie przykładowymi nazwami wyłączy bridge.

4. Sprawdź konfigurację i przeładuj: Narzędzia deweloperskie → YAML → sprawdź konfigurację,
   potem przeładuj skrypty, automatyzacje i liczniki albo zrestartuj Home Assistanta.

5. Załóż osobne konto MQTT dla pilota, żeby nie współdzielić poświadczeń z innymi klientami,
   i wpisz je na urządzeniu. Danych dostępowych nie trzyma się w tym repozytorium.

## Sprawdzenie bez pilota

Narzędzia deweloperskie → Akcje → `script.multimedia_remote_publish`. Potem w ustawieniach
integracji MQTT nasłuchaj tematu `multimedia_remote/v1/+/state` — powinien przyjść pełny
snapshot z rosnącą rewizją.

Polecenie sprawdzisz publikując na `multimedia_remote/v1/<identyfikator>/command`:

```json
{ "id": "test-1", "action": "z906.volume.set", "args": { "percent": 30 } }
```

## Zakres i granice

Pakiet obsługuje **Z906**, **Apple TV** i **klimatyzację**. Sekcji `tv` nie publikuje, więc pilot pokaże na
ekranie TV pasek o braku świeżego stanu — to zachowanie poprawne, a nie usterka. Z906 pilot
i tak czyta wprost z kontrolera (ADR 0006); z tego pakietu bierze sekcje `appletv` i `climate`. Encje LG
nie są jeszcze potwierdzone i zgadywanie ich nazw jest w tym projekcie zabronione.

`source` i `effect` publikujemy jako **bieżącą opcję z listy**, a nie wewnętrzny identyfikator
wejścia. Pilot dopasowuje tę wartość do `source_options` i `effect_options`, żeby zaznaczyć
właściwy segment; identyfikator w rodzaju `input_2` nie pasowałby do żadnej opcji i ekran nie
zaznaczyłby niczego. Przykład w kontrakcie bywa pod tym względem mylący.

Polecenia spoza zakresu trafiają do dziennika jako ostrzeżenie, zamiast cicho znikać.
