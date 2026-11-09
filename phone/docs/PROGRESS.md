# Stan prac — pilot na telefonie

Plik dla kolejnej sesji (człowieka albo AI). Aktualizować po każdym etapie.

## Decyzja 2026-11-07: aplikacja natywna (`native/`)

Port LVGL przez SDL (`android/`, `compat/`, `platform/`) działał, ale interfejs 480 × 800
skalowany ~2,25× był rozmyty i miał pasy. Użytkownik wybrał **natywną aplikację Android**:
Kotlin + Jetpack Compose, minSdk 23 (Galaxy S5), ten sam kontrakt MQTT co firmware.
Port SDL zostaje jako referencja (i symulator na Macu), rozwijamy `native/`.

### `native/` — co jest

Pakiet `app.couchpad.pilot` (refaktor 2026-11-08: logika bez UI wydzielona i pokryta testami
JVM, ekrany w pakietach według zakładek, wspólne kontrolki w `ui/components`).

- `MainActivity.kt` — okno (nad blokadą, wycięcie kamery, bez paska stanu), wibracja,
  start/stop połączenia. `AppGraph.kt` — repozytorium na proces i sygnał „wróć na Start”.
  `WindowDisplay.kt` — jasność, wygaszanie i orientacja okna (odpowiednik `board`).
- `data/` — bez Compose:
  - `mqtt/MqttClient.kt` — własny klient MQTT 3.1.1 (testament, QoS 0/1, keepalive,
    ponawianie, wysyłka w osobnym wątku); `mqtt/MqttCodec.kt` — kodowanie pakietów.
  - `PilotRepository.kt` — jedno połączenie: Z906 wprost (ADR 0006), Glasshouse wprost
    (ADR 0008), kontrakt v1 dla Apple TV, klimy, świateł, pogody i kanałów. Puste
    identyfikatory Z906 i pilota wykrywa sam z wiadomości retained u brokera.
  - `Contract.kt` (snapshot, kanały, pogoda), `TvAggregator.kt` (telemetria + tematy
    natychmiastowe TV), `Z906.kt` (tematy, wejścia, efekty, krok głośności),
    `Pending.kt` (klucze poleceń czekających na potwierdzenie), `Model.kt`.
  - `Settings.kt`, `Options.kt` (tabele opcji jak `storage::Pref`), `SettingsStore.kt`.
- `ui/` — Compose. Jednostka `u` = 1 px makiety (480 × 800 / 800 × 480), skalowana do ekranu.
  Ikony pixel-art z `firmware/docs/ux/mockup-pulpit/assets/pixel` (bez wygładzania),
  logotypy z SVG przez Inkscape, font Ubuntu.
  - `theme/` — palety, `C`, jednostki, tekst, ikony, wibracja.
  - `components/` — przyciski i zasilanie, nagłówki, listy wyboru i segmenty, tarcze
    (`Arc.kt`: wspólny łuk 270° dla jasności, temperatury i głośności), krzyżak i
    kołyski, suwak, szuflada przy krawędzi, próbki barwy, okna dialogowe.
  - `shell/` — `PilotApp` (ekrany, zakładki, wygaszacz), `ScreenPolicy` (styl wg godzin,
    tryb nocny, przyciemnianie), `TabBars` (dolny pasek, boczny pasek z wycięciem
    kamery), `WakeGate` („Łączenie z domem”).
  - `start/`, `tv/`, `appletv/`, `audio/`, `home/` (pokoje, pokój, lampa, żarówka),
    `climate/`, `settings/` — ekrany.
- Po każdym powrocie na wierzch: połączenie i świeży stan; ekrany urządzeń zasłania
  „Łączenie z domem”, dopóki stan nie przyjdzie (ADR 0005). W tle rozłącza się.

### Budowa

```bash
cd native
export JAVA_HOME=$(brew --prefix openjdk@17)/libexec/openjdk.jdk/Contents/Home
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties
./gradlew assembleDebug && adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n app.couchpad.pilot/.MainActivity
./gradlew testDebugUnitTest   # testy JVM: kodek MQTT, kontrakt, TV, Z906, reguły ekranu
```

### Stan (2026-11-07)

- Na M23 połączona z brokerem osobnym kontem MQTT (dane logowania tylko na telefonie).
- Wszystkie ekrany sprawdzone na prawdziwym stanie w pionie i w poziomie (boczny pasek
  zakładek, ADR 0013).
- Ustawienia 1:1 z płytki: Wi-Fi (system), MQTT, Ekran (orientacja auto/pion/poziom/
  poziom odwr., styl ciemny/jasny/wg godzin, jasność, tryb nocny, wygaszanie,
  przyciemnianie), Dźwięk (limit), Pokój i czas (nazwa, strefa), Klawiatura i gładzik
  (na żywo, wysuwanie, czułość), Diagnostyka, O urządzeniu (restart, fabryczne).
  Styl klawiatury (QWERTY/Apple TV) pominięty — tekst wpisuje się klawiaturą telefonu.

### Następne kroki

1. ~~Ekran klimatyzacji~~ — zrobiony (Dom → Klimatyzacja): tarcza temperatury z zakresem
   i krokiem z encji, tryby, moc nawiewu, żaluzje pion/poziom; sprawdzony w obu orientacjach
   na stanie z HA (bez włączania klimy).
   Zakładka Dom przebudowana 1:1 wg `firmware/docs/ux/home-screen.md`: Dom (lampki na
   kafelkach, pasek/pigułka klimy) → Pokój (pasek pokoi, karty Lampa i Klimatyzacja w
   salonie, „Brak encji”) → Lampa (tarcza 270° 5–100 z −/+, 7 próbek barwy, efekt z ikoną,
   żarówki) → Żarówka. W poziomie od pokoju w głąb: szuflady — zakładki z lewej, pokoje z
   prawej (uchwyt, stuknięcie albo przeciągnięcie). Prawa krawędź w poziomie to dół
   telefonu z obowiązkowym gestem Samsunga, dlatego pas uchwytu ma 48 px.
2. ~~Lista wyboru~~ — jak na płytce: w pionie pełny ekran, w poziomie panel 380 px z prawej;
   karty z ikoną i „aktywny”, wybór zamyka listę (efekty, źródła TV i Z906, opcje klimy).
   Pas przy przedniej kamerze (poziom): okno rysuje w wycięciu (SHORT_EDGES), boczny
   pasek zakładek stoi przy krawędzi wyświetlacza, a ikona na wysokości kamery jest
   odsunięta o szerokość wycięcia (`ui/shell/TabBars.kt`, `SideRail`); ekrany bez paska omijają pas.
3. ~~Tryb „tylko pilot”~~ — aplikacja jest launcherem (filtr HOME): startuje z telefonem,
   Home i każde zapalenie ekranu wracają na Start, okno pokazuje się nad ekranem blokady
   (`setShowWhenLocked`), Wstecz na Starcie niczego nie zamyka. Ustawienia → Telefon: wybór
   ekranu głównego (systemowy), ustawienia Androida, lista aplikacji. Domyślny launcher
   wybiera użytkownik — aplikacja go nie przestawia.
4. Galaxy S5: test na Androidzie 6.
5. ~~Efekty lampy jako kafle~~ (2026-11-08) — najpierw w aplikacji, tego samego dnia także na
   panelu (`docs/ux/home-screen.md` §4.7). Na ekranie Lampa kafle jak tryby klimy
   (`ui/home/EffectTiles.kt`): bieżący efekt na dużym kaflu, obok najczęściej wybierane efekty
   tej lampy (gdy efekt włączony, pierwszy to zawsze „Brak”), ostatni kafel „Więcej · N”
   otwiera pełną listę. Poziom jak na panelu (4 + Więcej na całą wysokość). **Pion różni się
   od panelu świadomie:** aplikacja ma dwa rzędy (3 + Więcej), panel pod tarczą 300 px mieści
   tylko jeden (2 + Więcej). Liczniki wyborów są tylko w telefonie (`SettingsStore`, klucz
   `effect_usage`, JSON lampa → efekt → liczba; kasuje je reset fabryczny), bez nowych tematów
   MQTT; remis rozstrzyga kolejność z HA (`data/EffectUsage.kt`, test `EffectUsageTest`).
   Makieta: `docs/mockups/light-effects-tiles.html`.
6. ~~Karta „W pokoju”~~ (2026-11-09, [ADR 0017](../../docs/architecture/adr/0017-room-climate-card.md))
   — 1:1 z panelem: temperatura i wilgotność czujnika pokoju z `indoor.rooms` w wiadomości
   pogody (`Contract.parseWeather`, `Weather.room`, test w `ContractTest`), ikony
   `px_sensor_temp` / `px_sensor_humidity` (48 px), bez oceny słowem (usunięta 2026-11-09). Pion: nad
   „URZĄDZENIA”, poziom: pierwsza kolumna 210 px. Karta klimy w salonie ma ikonę
   klimatyzatora `px_dev_aircon`. `tools/screenshots.sh`: w pionie lampa i klima zjechały
   w dół (nowe tapnięcia 480 945 i 482 1245).
7. ~~Grzejniki~~ (2026-11-09, [ADR 0018](../../docs/architecture/adr/0018-radiators.md)) — sekcja
   `radiators` snapshotu (najwyżej cztery, `Contract.radiator`, test w `ContractTest`), akcje
   `radiator.power.set` / `mode.set` / `target.set`, oczekiwanie `Pending.Radiator`. Karta
   „Grzejnik” w pokoju (studzienka `px_dev_radiator` / `_idle`, „w pokoju” tylko z czujnika
   pokoju) i ekran `ui/home/RadiatorPage.kt`: tarcza 5–30 ° z łukiem od niebieskiego do
   czerwieni (`Dial(brush, marker)`) i kropką temperatury pokoju, kafle Tryb / Teraz / Szybko
   (ikony `px_trv_*`). W poziomie salon ma trzy karty urządzeń: „W pokoju” zwęża się do 160 px
   (mniejsze liczby), układ kart bez zmian — zasilanie lampy w prawym górnym rogu; karty
   w poziomie mają strzałkę na dole po prawej. Do zrobienia: tapnięcia grzejnika
   w `tools/screenshots.sh` i odświeżenie zrzutów (stukanie w telefon zablokował klasyfikator
   trybu auto — trzeba zgody użytkownika). Ekranu grzejnika ani lampy w kuchni na żywych danych
   w telefonie po ostatnich poprawkach nikt jeszcze nie oglądał.
8. ~~Lampa w kuchni~~ (2026-11-09) — bez zmian w aplikacji: bridge podaje drugą pozycję `lights`
   (`room: kuchnia`, `id: lampa_kuchnia`), a aplikacja szuka świateł po pomieszczeniu.

---

# Historia: port LVGL przez SDL

## Zrobione (2026-11-07)

1. Warstwa zgodności `compat/` — ESP-IDF/FreeRTOS na std::thread, NVS w pliku, własny
   klient MQTT 3.1.1 (TCP, testament, QoS 0/1, keepalive, ponawianie) z API esp-mqtt.
2. `platform/` — board.hpp na SDL2 (render DIRECT RGB565 → tekstura → skalowanie z
   zachowaniem proporcji), Wi-Fi i zegar z systemu, `app_main.cpp` jak `main/main.cpp`.
3. **Symulator na Macu kompiluje się i rysuje ekran startowy identyczny z płytką.**
   Bez skonfigurowanego brokera kafelki pokazują „brak połączenia” — tak ma być.
4. Mac z uszkodzonymi Command Line Tools: CMakeLists.txt sam dodaje `-nostdinc++` i
   ścieżkę libc++ z SDK.

5. **APK działa na Galaxy M23 (SM-M236B, Android 14)**: ekran startowy, dotyk, ekran
   „Brak połączenia z domem”, Ustawienia (Wi-Fi: „Sieć telefonu · połączono”, MQTT: brak).
   Interfejs 480 × 800 skalowany do 1080 × 1800, pasy u góry i na dole.
6. Dotyk ma kolejkę zdarzeń — szybkie stuknięcie (np. `adb shell input tap`) nie przepada.

## Budowa APK

```bash
./tools/fetch-sdl.sh                       # raz: SDL2 2.30.9 do third_party/SDL2
cd android
export JAVA_HOME=$(brew --prefix openjdk@17)/libexec/openjdk.jdk/Contents/Home
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
adb shell am start -n app.couchpad.pilot/.PilotActivity
adb logcat 'pilot:*' 'SDL:*' '*:S'
```

SDK: `platforms;android-34`, `build-tools;34.0.0`, `ndk;27.2.12479018`, `cmake;3.22.1`.

## Następne kroki

1. ~~APK na M23~~ — zrobione.
2. Wpisać broker MQTT w *Ustawienia → MQTT* na telefonie i sprawdzić połączenie z HA.
3. Ostrość: renderować w wyższej rozdzielczości (2×) albo skalować fonty.
4. Jasność ekranu przez `WindowManager.LayoutParams.screenBrightness` (JNI).
5. Wybudzanie: po powrocie aplikacji na pierwszy plan wymusić świeży snapshot (ADR 0005).
6. Galaxy S5: Android 6 (API 23) — sprawdzić `minSdkVersion` i SDL2.

## Uwagi

- Aplikacja jest w katalogu `phone/` repozytorium firmware'u (od 2026-11-08; wcześniej
  osobne repozytorium obok). Firmware od niej nie zależy i nie jest przez nią zmieniany.
- LVGL (port SDL) bierzemy z `../managed_components/lvgl__lvgl` (9.2.2).
- `platform/lv_kconfig_phone.h` to kopia opcji `CONFIG_LV_*` z sdkconfig firmware'u.
