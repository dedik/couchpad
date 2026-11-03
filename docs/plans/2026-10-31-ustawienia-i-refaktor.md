# Plan: ekran WIĘCEJ (ustawienia) i refaktor przed kolejnymi funkcjami

_Szkic z 2026-10-31 do akceptacji. Nic z tego nie jest jeszcze zrobione._

## Kontekst

- **Ekran WIĘCEJ ma siedem wierszy, ale działają tylko trzy:** Wi-Fi, MQTT i Symulacja.
  - Jasność, Uśpienie, Diagnostyka i O urządzeniu to atrapy bez akcji (`screen_more.cpp:74-88`).
  - Jasność (100 %) i uśpienie (30 s) są zaszyte w kodzie.
  - Wersja „v0.1” to zwykły napis.
- **Przed nami TV LG przez HA, bateria, OTA i biblioteka Emby.** Przegląd kodu pokazał kilka miejsc, które warto naprawić wcześniej, bo każda z tych funkcji by je powieliła albo się o nie potknęła.

Plan ma dwie części:
- **A:** docelowy ekran ustawień.
- **B:** refaktor, ułożony tak, żeby A i TV budować już na nowym fundamencie.

---

## Część A — ekran WIĘCEJ

### Układ

Lista w sekcjach, każdy wiersz otwiera podstronę (`widgets::make_page`), jak dziś Wi-Fi i MQTT. Zostaje zasada z makiety: szósty ekran nie dostaje zakładki, tylko wiersz tutaj, a cel dotyku ma co najmniej 68 px.

```text
POŁĄCZENIA
  Wi-Fi                     nazwa sieci · siła sygnału        (jest)
  Home Assistant i MQTT     broker, Z906, identyfikator pilota (jest, porządki)
EKRAN
  Jasność                   suwak 10–100 %                     NOWE
  Wygaszanie                15 s / 30 s / 1 min / 2 min / nigdy NOWE
  Tryb nocny                ciemniej w wybranych godzinach     NOWE
  Ekran startowy            TV / Apple TV / Głośniki / Dom / ostatni NOWE
STEROWANIE
  Klawiatura                Apple TV / QWERTY · tekst na żywo  (jest w edytorze, dochodzi tu)
  Gładzik Apple TV          czułość · duży / mały układ        NOWE
POKÓJ I CZAS
  Nazwa pokoju              „SALON” w nagłówku                 NOWE
  Strefa czasowa            lista stref                        NOWE
SYSTEM
  Diagnostyka               stan łączy, sygnał, pamięć, czas pracy NOWE
  O urządzeniu              wersja, data kompilacji, MAC, płytka NOWE
  Tryb prób                 dane udawane + panel symulacji      (przeniesione z dwóch miejsc)
  Uruchom ponownie                                              NOWE
  Przywróć ustawienia fabryczne                                 NOWE
```

### Szczegóły ustawień

| Ustawienie | Zachowanie | Uwagi techniczne |
| --- | --- | --- |
| **Jasność** | Suwak 10–100 % co 10, podgląd od razu, zapis po puszczeniu palca. | **Pułapka:** `bsp_display_backlight_on()` zawsze ustawia 100 %, więc po każdym wybudzeniu jasność wracałaby do 100 %. `board::set_display_on(true)` musi przywracać zapisaną wartość. Dolna granica 10 %, żeby nie dało się „zgasić” ekranu na stałe. |
| **Wygaszanie** | 15 s, 30 s, 1 min, 2 min, nigdy. | Dziś `kIdleTimeoutMs` w `ui.cpp:11`. „Nigdy” tylko przy zasilaniu z USB — po dodaniu baterii opcja znika albo ostrzega. |
| **Tryb nocny** | Od–do (np. 22:00–6:00) jasność X %. | Płytka nie ma czujnika światła, więc „automatyczna po zmroku” z makiety może działać tylko według godziny z SNTP. Bez znanego czasu tryb nocny jest wyłączony. |
| **Ekran startowy** | Zakładka po starcie i po wybudzeniu; „ostatni” pamięta, gdzie skończyłeś. | Dziś zawsze TV (`ui.cpp:116`). |
| **Klawiatura** | Styl Apple TV / QWERTY oraz tekst na żywo. | To samo NVS co dzisiejsze chipy w edytorze (`kb_qwerty`, `kb_live`). Chipy zostają, tu jest drugi dostęp. |
| **Gładzik** | Czułość: mała / średnia / duża (krok 34 / 26 / 20 px). Układ: duży gładzik bez biblioteki / z biblioteką. | Dziś `kTrackpadStep` i stała `kCompactLayout` w kodzie. |
| **Nazwa pokoju** | Tekst w nagłówku zakładek („SALON”). | Dziś zaszyty w `shell.cpp:23-27`. |
| **Strefa czasowa** | Kilka stref (Polska domyślnie, UTC, Londyn, …). | Dziś `kTimezone` w `time_sync.cpp:15`. Lista stała w firmware, bez wpisywania napisu POSIX. |
| **Diagnostyka** | Stan Wi-Fi (sygnał w dBm, odświeżany co kilka sekund), IP, Z906 i HA osobno (połączony / błąd / ostatni stan sprzed X s), rewizja snapshotu, ostatnie polecenie, wolna pamięć, czas pracy, czas od synchronizacji zegara. | RSSI odczytuje dziś tylko raz przy łączeniu; trzeba odpytywać `esp_wifi_sta_get_ap_info`, ale tylko gdy strona jest otwarta. |
| **O urządzeniu** | Wersja i data kompilacji z `esp_app_get_description()`, płytka, MAC, wersja firmware ESP32-C6. | Wersja z `PROJECT_VER` w `CMakeLists.txt` zamiast napisu „v0.1”. |
| **Tryb prób** | Przełącznik danych udawanych i panel symulacji w jednym miejscu. | Dziś przełącznik jest na stronie MQTT, a panel w osobnym wierszu. |
| **Uruchom ponownie** | Potwierdzenie, potem `esp_restart()`. | — |
| **Przywróć fabryczne** | Dwustopniowe potwierdzenie, kasuje NVS (Wi-Fi, MQTT, ustawienia), restart. | Najbezpieczniej przez nową funkcję `storage::factory_reset()`, a nie ręczne kasowanie kluczy. |

### Świadomie odłożone

Każda z tych rzeczy wymaga sprzętu albo ADR-u:

- **Bateria i oszczędzanie energii** (poziom, profil): MAX17048 jest tylko w planach obudowy, a uśpienie radia wydłuża wybudzenie do ~8 s (ADR 0005).
- **Aktualizacja OTA:** tablica partycji nie ma slotów OTA, więc potrzebna jest nowa tablica partycji, jednorazowe wgranie kablem i ADR. To mocny kandydat na następny duży etap: tvOS zmienia protokoły, a pilot bez OTA trzeba wgrywać przez USB.
- **Wybór aplikacji na kafelkach z pilota:** wymaga, żeby bridge publikował pełną listę aplikacji, czyli zmiany kontraktu.
- **Język i jasny motyw:** brak potrzeby, a koszt to fonty i podwójne style.

---

## Część B — przegląd kodu i refaktor

### Usterki znalezione przy okazji (naprawić najpierw)

1. **Z906 łamie zasadę „dotyk nie czeka na sieć”.**
   - `Z906Client::publish_value` woła blokujące `esp_mqtt_client_publish` z zadania UI (`z906_client.cpp:243`).
   - Przy słabym Wi-Fi tarcza głośności może zamrozić ekran.
   - Klient HA używa już nieblokującego `enqueue`.
2. **Odczyt NVS przy każdym rysowaniu.**
   - `render_more` i `render_mqtt` wołają `storage::simulation_enabled()`, czyli otwarcie, odczyt i zamknięcie NVS.
   - `render()` przerysowuje wszystkie ekrany po każdym snapshocie.
3. **Każdy snapshot zdejmuje wszystkie oczekiwania.**
   - Echo głośności z Z906 gasi „oczekuje” na kafelku Apple TV, zanim HA potwierdzi (`app_controller.cpp:80`).
   - Przy TV będzie to widać bardziej.
4. **Polecenia TV, scen i skrótów w trybie prawdziwym nigdzie nie idą.**
   - `LiveSource` wysyła je do klienta Z906, który je po cichu pomija.
   - TV pokazuje wtedy „oczekuje”, a po 3 s „brak potwierdzenia”.
5. **Drobne:**
   - `static char client_id` w `Z906Client::start` jest wspólny dla instancji.
   - `Z906Client::error_` jest zapisywany z zadania MQTT i czytany z UI bez atomowości.
   - Klient HA nie zeruje `last_revision_` po ponownym połączeniu, więc reset licznika w HA zamroziłby Apple TV do restartu pilota.
   - Powrót z trybu prób nie zatrzymuje zadania symulacji.
   - `storage::init()` jest wołane dwa razy.

### Refaktory według wartości

| # | Zmiana | Po co | Ryzyko |
| --- | --- | --- | --- |
| 1 | **Jedna sesja MQTT** (`transport::MqttSession`): połączenie, router tematów, sklejanie fragmentów, nieblokująca wysyłka, błędy. Z906 i HA stają się cienkimi kodekami. | ~150 zduplikowanych linii mniej. Naprawia usterki 1 i 5. TV LG to ~50 linii kodeka zamiast trzeciego klienta i trzeciego połączenia TCP z brokerem. | średnie |
| 2 | **Zdarzenia per urządzenie zamiast kopii całego snapshotu** (~2 kB). Oczekiwania czyszczone per sekcja. | Koniec kopii 2 kB na 8–12 kB stosie LVGL (raz już był przez to restart w pętli) i kolejki Z906 zajmującej ~24 kB. Naprawia usterkę 3. Warunek przed dodaniem TV. | średnie |
| 3 | **`Device` w zamiarze i tabela tras w `LiveSource`** zamiast porównań napisów `target == "appletv"`. | Dodanie TV to jeden wiersz tabeli; nieobsłużone zamiary są jawne (usterka 4). | niskie |
| 4 | **Kodek kontraktu v1 jako czyste funkcje** (`parse_state`, `encode_command`, kodek tematów Z906) **+ testy na komputerze**. | Dziś testów jest zero. To najczęściej zmieniane miejsca (TV, Emby). Testy: AppState, AppController, kodeki, układy klawiatury. | niskie |
| 5 | **Usługa ustawień w `application`** z pamięcią podręczną i pomocnikiem NVS (RAII, `get`/`set` z domyślną wartością, wersja schematu). | Fundament części A. Naprawia usterkę 2. Usuwa wołania `storage` z `widgets.cpp` i ekranów. | niskie |
| 6 | **Podział `widgets.cpp` (1821 linii)**: kontrolki, tarcza i poziom, nawigacja (krzyżak, gładzik, gesty), edytor z klawiaturą. Jeden pomocnik `bind<T>()` zamiast ośmiu wariantów wiązania callbacków. `make_sheet()` na nakładki. | Część A doda kolejne widżety (suwak jasności, wybór z listy, potwierdzenie); łatwiej je dołożyć do mniejszych plików. | niskie, mechaniczne |
| 7 | **Usunięcie martwej biblioteki** „Nowe filmy / Nowe seriale” (połowa `screen_appletv.cpp`, `AtvPlay`, 12 plakatów ~2 tys. linii). Symulacja za opcją Kconfig. | Emby i tak zaprojektujemy od nowa z kontraktu. Mniej flasha i globali. | niskie |
| 8 | **UI bez zależności od `transport` i `storage`**: model widoku z `application` (stan łączy, sygnał, błędy, zegar), przełączanie źródeł w `application`, tabela urządzeń zamiast `switch (ScreenId)` w nagłówku. | Jedno miejsce na podpięcie baterii, OTA i oszczędzania energii. | średnie |

Do tego: stałe czasowe w jednym `core/config.hpp` (część w Kconfig) oraz poprawienie nieaktualnych komentarzy, które wciąż opisują symulację jako główne źródło.

---

## Proponowana kolejność

Każdy etap kończy się kompilacją, wgraniem za zgodą i sprawdzeniem na pilocie, a potem osobnym commitem.

1. **Etap 0 — szybkie poprawki** (usterki 1, 2, 5, bez zmian architektury). Wynik: tarcza głośności nie blokuje UI, a rysowanie przestaje czytać NVS.
2. **Etap 1 — porządki:**
   - refaktor 7 (martwy kod);
   - refaktor 6 (podział widżetów);
   - refaktor 5 (usługa ustawień);
   - stałe w `core/config.hpp`.

   Zachowanie się nie zmienia.
3. **Etap 2 — ekran WIĘCEJ, partia pierwsza:**
   - jasność (z naprawą pułapki z podświetleniem), wygaszanie, ekran startowy;
   - O urządzeniu z prawdziwą wersją, uruchom ponownie, przywróć fabryczne;
   - tryb prób w jednym miejscu.
4. **Etap 3 — ekran WIĘCEJ, partia druga:**
   - diagnostyka;
   - klawiatura i gładzik;
   - nazwa pokoju, strefa czasowa, tryb nocny.
5. **Etap 4 — fundament pod TV:** refaktory 1, 2 i 3, potem testy z refaktoru 4. Po nim TV LG to głównie kodek i bridge.
6. **Etap 5 (osobny ADR) — OTA.** Potem bateria, gdy będzie sprzęt.

Refaktor 8 można rozłożyć: przy etapie 3 przenieść stan łączy do modelu widoku, a resztę przy etapie 4.

## Weryfikacja

- `idf.py build` bez nowych ostrzeżeń.
- Po etapie 4 cel testów na komputerze (`ctest`) dla domeny i kodeków.
- Na pilocie:
  - każde ustawienie przeżywa restart;
  - jasność wraca po wybudzeniu;
  - „przywróć fabryczne” prowadzi do ekranu konfiguracji Wi-Fi;
  - tarcza głośności nie zacina się przy słabym sygnale;
  - log startu bez restartów i bez przepełnienia stosu.

## Pytania do decyzji

1. Czy przyjmujesz listę ustawień z części A? Czego brakuje, a co jest zbędne?
2. Czy usuwamy bibliotekę „Nowe filmy / Nowe seriale” na dobre (refaktor 7)? Po Emby zaprojektujemy ją od nowa.
3. Kolejność: najpierw ekran ustawień (etapy 0–3), czy najpierw fundament pod TV (etap 4)?
4. Czy OTA ma być następnym dużym etapem po ustawieniach?
