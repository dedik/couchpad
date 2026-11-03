# Apple TV — model Home Assistant

_Odczyt tylko do odczytu z 2026-10-31 przez REST API Home Assistanta (`/api/states` oraz
`media_player.browse_media` dla `apps`). Integracja sparowana._

Pilot nie używa tych identyfikatorów bezpośrednio — rozmawia kontraktem `multimedia_remote/v1`
([ADR 0007](../architecture/adr/0007-apple-tv-via-home-assistant.md)). Tabela jest referencją
dla bridge'a w `homeassistant/multimedia_remote.yaml` i dla kafelków w
`components/ui/screens/screen_appletv.cpp`.

## Parowanie

1. **Ustawienia → Urządzenia i usługi → Dodaj integrację → Apple TV.**
2. Wybierz urządzenie i wpisz PIN-y pokazane na telewizorze (Companion, AirPlay).
3. Na Apple TV: **Ustawienia → AirPlay i HomeKit → Zezwalaj na dostęp: Każdy w tej samej sieci**
   albo zostaw „Tylko ten dom”, jeśli HA jest w tym samym domu Apple.

## Encje

| Funkcja | Encja Home Assistant | Uwagi |
| --- | --- | --- |
| Odtwarzacz, aplikacje, zasilanie | `media_player.living_room_apple_tv` | atrybuty `app_id`, `app_name`, `source_list`; `turn_on`/`turn_off` |
| Pilot | `remote.living_room_apple_tv` | wyłącznie `remote.send_command`; jego `turn_on`/`turn_off` nie usypia urządzenia |
| Klawiatura aktywna | `binary_sensor.living_room_apple_tv_keyboard_focus` | `on`, gdy na Apple TV jest pole tekstowe |
| Wpis konfiguracji | `<id wpisu integracji>` | wymagany przez `apple_tv.set_keyboard_text`; zmienia się po ponownym parowaniu |

`remote.living_room_apple_tv` ma stan „on” tylko wtedy, gdy integracja jest połączona z Apple TV.
Bridge liczy z niego `appletv.available`: bez połączenia polecenia kończą się w HA błędem
`Unable to send commands, not connected`, więc pilot pokazuje brak świeżego stanu.

**Zawieszone połączenie (2026-10-31).** Integracja rozłączyła się o 21:46 i sama nie wróciła,
choć Apple TV odpowiadało w sieci (porty 49153 i 7000 otwarte). Pomogło przeładowanie wpisu
konfiguracji (Urządzenia i usługi → Apple TV → Uruchom ponownie). Przyczyny log nie pokazał;
jeśli się powtórzy, włączyć logowanie debug dla `pyatv` i `homeassistant.components.apple_tv`.

## Aplikacje na kafelkach

Odczyt: **Narzędzia deweloperskie → Stany → `media_player.…`**. Otwórz każdą aplikację na
Apple TV i przepisz `app_id` oraz `app_name` dokładnie co do znaku. Sprawdź też, pod jaką
nazwą aplikacja stoi w `source_list`.

| Kafelek | `app_name` | pozycja w `source_list` | `app_id` (bundle id) | Uruchamianie |
| --- | --- | --- | --- | --- |
| Infuse | Infuse | `Infuse` | `com.firecore.infuse` | `play_media` — potwierdzone 2026-10-31 |
| YouTube | YouTube | `YouTube` | `com.google.ios.youtube` | `play_media` (nie testowano osobno) |
| WP Pilot | — | `Pilot WP` | `pl.pilotwp.appletv` | `play_media` (nie testowano osobno) |

Uwaga: aplikacja WP Pilot występuje na Apple TV jako **„Pilot WP”**. Nazwy w `source_list`
mogą zawierać niełamliwe spacje (np. `App\u00a0Store`), więc porównujemy je co do bajtu.

Pełna lista aplikacji tego Apple TV (21) jest dostępna przez `media_player.browse_media`
z `media_content_type: apps`, `media_content_id: apps`; `media_content_id` dziecka to bundle id.

W kolumnie „Uruchamianie” zapisz, co zadziałało w Narzędziach deweloperskich:

- `media_player.play_media` z `media_content_type: app`, `media_content_id: <app_id>` (preferowane);
- albo `media_player.select_source` z `source: <pozycja z source_list>`.

## Ukryte gesty pilota Siri

Integracja HA (`remote.send_command`) zna tylko pojedyncze stuknięcie i przytrzymanie
(`hold_secs` ≥ 1 → `InputAction.Hold`). Podwójne stuknięcie daje się złożyć z dwóch
stuknięć bez przerwy. Sprawdzone na tym Apple TV 2026-10-31:

| Gest | Skutek | Wywołanie HA | Wynik |
| --- | --- | --- | --- |
| Home ×2 | przełącznik aplikacji | `command: home`, `num_repeats: 2`, `delay_secs: 0` | działa |
| Home — przytrzymanie | Centrum sterowania | `command: home`, `hold_secs: 1` | działa |
| OK — przytrzymanie | menu kontekstowe ikony | `command: select`, `hold_secs: 1` | działa |
| Menu — przytrzymanie | ekran główny | `command: top_menu` | nie testowano |

Na pilocie te same gesty leżą tam, gdzie na pilocie Siri: przycisk Home rozpoznaje ×2
(okno 300 ms) i przytrzymanie (400 ms), środek gładzika — przytrzymanie. Kontrakt przenosi
je jako `appletv.navigate` z `"action": "double" | "hold"`.

Dostępne, nietestowane: `play_pause`, `skip_forward`, `skip_backward`, `channel_up`,
`channel_down`, `guide`, `screensaver`, `control_center`. Niedostępne przez HA: Siri,
jednoczesne Home + Menu (restart).

## Sprawdzenie przed włączeniem bridge'a

- [x] `remote.send_command` z `command: right` przesuwa zaznaczenie na Apple TV (2026-10-31).
- [x] `media_player.turn_off` usypia, `media_player.turn_on` budzi Apple TV (2026-10-31).
      **Nie używać `remote.turn_off/turn_on`:** zmienia tylko stan w HA (encje pokazują
      `off`), a Apple TV dalej działa.
- [x] `media_player.play_media` z bundle id uruchamia aplikację (Infuse, 2026-10-31).
      **Uwaga:** atrybut `app_id` nie przełączył się na Infuse przez co najmniej 8 s —
      HA odświeża go z „now playing”. Podświetlenie aktywnego kafelka na pilocie może
      więc pokazywać poprzednią aplikację.
- [x] Na polu wyszukiwania YouTube `binary_sensor` przechodzi na `on`, a
      `apple_tv.set_keyboard_text` wpisuje tekst, także z polskimi znakami („Kanał Zero”, 2026-10-31).
