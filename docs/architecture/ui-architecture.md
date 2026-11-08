# Architektura UI

[English](ui-architecture.en.md)

## Zasada projektowa

Interfejs ma wyglądać jak produkt konsumencki, nie jak dashboard administratora. Ekran dotykowy jest jedynym głównym wejściem w v1, dlatego akcje częste muszą być duże, jednoznaczne i dostępne bez przewijania.

## Warstwy

```text
theme/       kolory, typografia, odstępy, promienie, stany aktywne
components/  przycisk ikona+tekst, kafelek urządzenia, suwak, przycisk nawigacji
screens/     Home, TV, Z906, Home Assistant, Settings, Diagnostics
binding/     AppState → właściwości istniejących obiektów LVGL
```

Widok wysyła wyłącznie `UiIntent`. Kontroler aplikacji waliduje go, aktualizuje stan oczekujący i przekazuje właściwe `Command` do MQTT. Widziany stan urządzenia pochodzi wyłącznie z `DeviceSnapshot`.

## Nawigacja podstawowa

Na ekranach codziennego sterowania zawsze widoczny jest dyskretny dolny pasek z dwiema zakładkami: `TV` i `GŁOŚNIKI`. Wybrana zakładka wskazuje aktualny widok i przełącza bezpośrednio między ekranami TV oraz Z906 — bez powrotu do ekranu Home i bez dodatkowego potwierdzenia. Pasek jest wspólnym komponentem LVGL, tworzonym raz i aktualizowanym razem z bieżącym ekranem.

W orientacji poziomej ([ADR 0013](adr/0013-landscape-orientation.md)) te same zakładki stoją w pionowym pasku z lewej (88 px), a nagłówek z nazwą ekranu i zasilaniem zostaje nad treścią.

## Ekrany MVP

| Ekran | Cel | Najważniejsze elementy |
| --- | --- | --- |
| Home | natychmiastowa kontrola | status TV/Z906, mute, głośność, ostatnie skróty |
| TV | codzienne sterowanie | zasilanie, D-pad/OK, Home, Back, media |
| Z906 | dźwięk | duża głośność, mute, źródło, efekt, ustawienia kanałów |
| HA | krótkie akcje | świadomie wybrane skróty, nie pełny dashboard |
| Settings | konfiguracja | Wi-Fi, MQTT, jasność, informacje o wersji |

Widok Z906 używa zaakceptowanego układu **Soundstage**: centralnego pierścienia głośności, poziomego wyboru źródła, trzech kafli kanałów z małymi tarczami i −/+ (krok o poziom wzmacniacza), wyboru efektu oraz statusu dekodowania. Nie zastępujemy go uproszczoną kartą statusową.

## Zasady wydajności

- Ekran pracuje w RGB565; grafiki są wcześniej przeskalowane i zapisane w formacie właściwym dla LVGL.
- Nie przebudowujemy całego ekranu po otrzymaniu stanu. Aktualizujemy tylko zmienione komponenty.
- Nie tworzymy ani nie usuwamy obiektów LVGL przy każdym odświeżeniu; ekrany są tworzone raz i wiązane ze stanem.
- Animacje są krótkie, opcjonalne i nie są wymagane do przekazania informacji.
- Żadnego I/O, oczekiwania na MQTT ani alokacji dużych obiektów w callbacku dotyku.

Projekt graficzny powstaje najpierw jako makieta 480 × 800, potem jest odwzorowany z tych samych tokenów w LVGL. To umożliwia zmianę stylu bez naruszania logiki sterowania.

Szczegółowy, zatwierdzony układ ekranów TV i Z906 znajduje się w [`../ux/tv-and-z906-ui.md`](../ux/tv-and-z906-ui.md). Ta specyfikacja jest wiążąca dla implementacji komponentów LVGL.

## Mapa kodu interfejsu

Skrót dla kolejnej osoby lub AI: gdzie co leży i jakim wzorem dokłada się ekran,
żeby nie czytać za każdym razem całego `components/ui`.

### Przepływ stanu

1. Dotknięcie → handler ekranu buduje `domain::UiIntent` (`domain/intent.hpp`) i woła
   `ui::dispatch()`. Handler nigdy nie zmienia stanu sam.
2. `AppController::dispatch()` (`application/app_controller.cpp`) oddaje zamiar źródłu
   i oznacza sterowanie jako `Pending` (`AppState::mark_pending`: `requested` = `intent.text`,
   `requested_value` = `intent.value`, `requested_number` = `intent.number`).
3. Źródło (`SnapshotSource`) odpowiada snapshotem. Każdy snapshot o wyższej rewizji
   zastępuje `AppState::snapshot()` i **czyści wszystkie oczekiwania**. Brak snapshotu
   w `kConfirmTimeoutMs` daje `Failed`.
4. `ui::render()` (`ui.cpp`) przerysowuje wszystko ze stanu. To jedyna droga danych na
   ekran; `render_*` muszą być tanie i idempotentne.

Źródło: `LiveSource`, który skleja
`Z906Source` (MQTT wprost, ADR 0006), `HaClient` (Apple TV, klimatyzacja i kanały,
kontrakt v1, ADR 0007/0009) oraz
`TvClient` (Glasshouse, ADR 0008). Rozdział zamiarów: `targets_tv`, `targets_appletv`,
`targets_home_assistant`, `targets_climate` w `domain/intent.hpp`.

Snapshot (`domain/snapshot.hpp`) ma ok. 3,6 kB. **Nie trzymaj go na stosie** w kodzie
wołanym z zadania LVGL (`pump()`, `poll()`) ani w handlerach MQTT (stos `mqtt_task`
ma 6 kB): zdarzenia są polami klas (`AppController::event_`, `HaClient::outgoing_`),
a klienci transportu wysyłają w kolejce tylko stan swojego urządzenia (`Z906Event` niesie
`Z906State`), nie cały snapshot. Kolejki FreeRTOS leżą w pamięci wewnętrznej.

### Pliki `components/ui`

| Plik | Zawartość |
| --- | --- |
| `include/ui/theme.hpp`, `theme.cpp` | tokeny z `pulpit.css`: kolory, promienie, odstępy, wymiary powłoki, fonty, wspólne style. Dwie palety (ciemna, jasna) o tych samych rolach; `theme::apply()` przełącza je w locie, zamieniając kolory ról w stylach wspólnych i lokalnych wszystkich obiektów. Ekrany nie definiują własnych kolorów — kolor spoza palety nie zmieni się ze stylem. |
| `include/ui/widgets.hpp` | cały katalog komponentów — zacznij od tego pliku |
| `widgets.cpp` | prymitywy (`make_box`, `make_text`, `make_bare_button`, `make_dot`, `make_chevron`, `set_visible`, `upper_pl`), sekcja, `make_fill`, `make_cap`, `Segmented`, `Tiles`, akcje, `ListRow`, `Readout`, `Banner` + `set_unavailable`, przycisk zasilania, `Page`, `ask_confirm`; poziom: `make_split`, `make_pane`, `make_spacer`, `make_header_segment`, `dock_sheet` |
| `widgets_controls.cpp` | wartości ciągłe: `Dial` (głośność), `Thermostat` (temperatura), `Level` (kanały Z906) |
| `widgets_nav.cpp` | krzyżak, kołyska VOL/P, gładzik Apple TV |
| `widgets_editor.cpp` | edytor tekstu z własną klawiaturą |
| `widgets_internal.hpp` | prywatne pomocniki widżetów (`plain`, `attach`, `icon_label`) — ekrany ich nie widzą |
| `ui_internal.hpp` | `Context` i struktury ekranów (`TvScreen`, `ClimateScreen`…), deklaracje `build_*`/`render_*`, `make_intent` (zamiar z rodzajem i kontrolką) |
| `shell.cpp` | powłoka: nagłówek, pięć zakładek (`kScreens`; w poziomie boczny pasek), nakładka wybudzania, miejsce w nagłówku dla ekranu (`make_header_slot`, `show_header_slot`) |
| `ui.cpp` | `dispatch`, `render`, wygaszanie i wybudzanie ekranu |
| `screens/screen_*.cpp` | po jednym pliku na ekran lub podstronę; `screen_start.cpp` to ekran startowy po starcie i wybudzeniu (ADR 0010) |
| `screens/home_*.cpp`, `home_internal.hpp` | zakładka Dom w częściach: `screen_home.cpp` (siatka pomieszczeń), `home_room.cpp` (podstrona pokoju i stos poziomów), `home_light.cpp` (światło, żarówka, lista efektów), `home_drawers.cpp` (szuflady w poziomie), `home_common.cpp` (katalog pokoi i efektów, dane wspólne) |
| `screens/climate_tile.cpp`, `climate_internal.hpp` | kafelek klimatyzacji na ekranie Dom; podstrona zostaje w `screen_climate.cpp`, nazwy trybów i biegów w `climate_internal.hpp` |

### Wzorce, z których składa się ekran

- **Ekran zakładki:** `build_x(context, parent)` w `widgets::make_section`, `render_x(context)`
  dopisany do `ui::render()`. Brak świeżego stanu: `make_banner` + `set_unavailable(section, …)`.
- **Podstrona** (wszystko głębiej niż zakładka): `widgets::make_page(title, on_back, ctx)` —
  pełny ekran na `lv_layer_sys` z Cofnij w nagłówku; `show_page(page, bool)`. Prawą stronę
  nagłówka (np. zasilanie) dokłada się do `lv_obj_get_parent(page.title)`. Podstrony
  rzadko używane buduje się przy pierwszym otwarciu (`ClimateScreen::built`) — wolnej
  pamięci wewnętrznej jest ok. 180 kB.
- **Ekran startowy** (ADR 0010) leży na `lv_layer_sys` nad podstronami; `show_start(true)`
  zamyka wszystkie podstrony i okna (`close_overlays` w `screen_start.cpp`). **Nowa
  podstrona musi zostać tam dopisana**, inaczej po wybudzeniu przykryje ekran urządzenia.
- **Wybór z długiej listy:** druga `Page` z `ListRow` na każdą opcję, bez ikony i strzałki
  (`screen_tv.cpp` → `picker`, `screen_climate.cpp` → `picker`). Lista zamyka się od razu,
  oczekiwanie widać na kontrolce, która ją otworzyła.
- **Wartość w toku:** `state.pending_of(control)` podaje, o co poproszono — rysuj ją zamiast
  wartości ze snapshotu, dopóki snapshot nie przyjdzie (`render_half` w `screen_tv.cpp`,
  `shown_target` w `screen_climate.cpp`). Widżet z gestem (`Dial`, `Thermostat`) ma
  pierwszeństwo, gdy `busy()`.
- **Wartości ciągłe:** zmiana względna, nigdy skok do palca; jedno polecenie po puszczeniu
  palca (lub po 0,7 s od ostatniego stuknięcia w −/+ na `Thermostat`).
- **Callbacki:** captureless lambda + `Context*` jako user data; wiersze list niosą własną
  strukturę `{row, context, index}`.
- **Klocki:** kształty składa się z prymitywów `widgets` (`make_box`, `make_text`…), a nie
  z lokalnych kopii w pliku ekranu; widoczność — `widgets::set_visible`, zamiar —
  `make_intent(kind, control)`. Ekran dłuższy niż ok. 600 linii dzieli się na pliki
  z prywatnym nagłówkiem `*_internal.hpp` (jak Dom i klimatyzacja).

### Pamięć

Pamięć wewnętrzna (~768 kB, z czego ~490 kB wolne po starcie) jest zasobem krytycznym:
trzymają ją stosy zadań, kolejki FreeRTOS i każda alokacja do 4 kB
(`SPIRAM_MALLOC_ALWAYSINTERNAL`). PSRAM (32 MB) jest prawie pusty. Zasady:

- obiekty LVGL idą do PSRAM samym alokatorem (`board/lv_mem_psram.c`) — nic nie trzeba robić;
- duży obiekt na cały czas pracy: `core::memory::make_in_psram<T>(...)`, nie zmienna globalna;
- bufory danych (JSON, plakaty, listy): `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`;
- po zmianie sprawdź linie `mem:` w logu startu (etapy i zapas stosów po 30 s);
- MQTT: jedno połączenie (`transport::MqttBus`). Nowy klient podpina się przez `attach()`
  przed `start()`, odsiewa swoje tematy i wysyła przez `bus.publish()` — nigdy własnym
  `esp_mqtt_client_init` ani `enqueue`;
- stos zadania w PSRAM (`xTaskCreatePinnedToCoreWithCaps`) tylko wtedy, gdy zadanie nie
  dotyka flasha (NVS, OTA).

### Orientacja pozioma

[ADR 0013](adr/0013-landscape-orientation.md). `theme::landscape()` mówi, który układ zbudować;
orientacja zmienia się tylko restartem, więc ekran buduje jeden z dwóch układów, a `render_*`
zostaje wspólny. Wzorce:

- **Ekran w dwóch kolumnach:** `widgets::make_split(section, gap)` i kolumny
  `widgets::make_pane(section, szerokość, gap)` (0 = reszta wiersza); `make_spacer` dopycha
  przyciski do dołu kolumny. Treść ma 672 × 383 px.
- **Kontrolki w nagłówku** (źródło i tryb obrazu TV, źródło i efekt Z906):
  `make_header_slot(context, szerokość)` przy budowie, `show_header_slot(context, slot, ekran,
  świeży)` w `render_*`, `widgets::make_header_segment` obniża segment do wiersza 64 px.
  Nagłówek leży poza sekcją ekranu, więc baner braku stanu go nie zasłania — handler musi sam
  sprawdzić świeżość stanu.
- **Listy wyboru:** `widgets::dock_sheet(page)` po `make_page` — w poziomie panel 380 px
  z prawej nad przyciemnieniem, w pionie bez zmian.
- **Podstrony ustawień:** zadokowane obok listy (`dock_settings_pages`), otwarta najwyżej
  jedna; nowa podstrona ustawień musi trafić do `settings_page()` w `screen_more.cpp`.
- **Etykiety z wielokropkiem** (`LV_LABEL_LONG_DOT`) potrzebują stałej szerokości. Z szerokością
  „wg treści” LVGL pokazuje samo „…” (pogoda, moc nawiewu), a w kolumnie z `flex_grow` potrafi
  przeliczać układ bez końca.

### Styl jasny i ciemny

Ustawienie *Więcej → Ekran → Styl*: ciemny (domyślny), jasny albo „wg godzin” — ciemny
w wybranym oknie (domyślnie 18–8), jasny poza nim (`Pref::ThemeMode`, `Pref::ThemeHours`,
`effective_theme()` w `screen_device.cpp`). `on_tick` sprawdza styl co sekundę, także
w uśpieniu. Zanim pilot zna godzinę, „wg godzin” daje ciemny. Nowy kolor dokłada się
do obu palet w `theme.cpp` (i do `pulpit.css` makiety); żadne dwie role jednej palety nie mogą
mieć tej samej wartości. Jedyny celowy kolor spoza palety to `theme::logo_plate()` — ciemna
płytka pod logotypami, bo TVP i WP Pilot są białe. Decyzja i odrzucone warianty:
[ADR 0012](adr/0012-light-and-dark-theme.md).

### Fonty i ikony

- Ubuntu 400/500/700 generuje `tools/gen-fonts.sh` (mapy bitowe w `ui/assets/fonts`).
  Duże odczyty mają osobny, mały zestaw znaków: `font_b46num`, `font_b64num` — cyfry,
  przecinek i pauza „—”. Inny znak w dużym odczycie wymaga dopisania go do `num`
  w skrypcie i ponownego uruchomienia.
- Ikony zakładek i kafelków startu: pixel art 48 × 48 z `docs/ux/mockup-pulpit/assets/pixel/`
  (`tools/draw_icons.py` → PNG → `tools/gen-assets.sh` → obrazy `I8`, 48 i 96 px).
- Ikony funkcyjne to wbudowane `LV_SYMBOL_*` (Montserrat). Zestawu z makiety (lucide:
  śnieżynka, płomień, wentylator…) jeszcze nie ma — dlatego segmenty trybów klimy są
  tekstowe. Ikony zakładek (48 px), kafelków startu (`icon_*_large`, 96 px) i logotypy robi
  `tools/gen-assets.sh`.

### Budowanie

```bash
export PATH="/opt/homebrew/opt/python@3.13/libexec/bin:$PATH"
. ~/esp/esp-idf/export.sh && idf.py build
```

Środowisko ESP-IDF 5.5 jest zainstalowane pod Pythonem 3.13; domyślny `python3` (3.15)
kończy `export.sh` błędem o braku wirtualnego środowiska. Uwaga: `export.sh … | tail`
zwraca kod 0 nawet wtedy, gdy `idf.py` nie istnieje — sprawdzaj wynik, nie kod.
