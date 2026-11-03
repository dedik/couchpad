# Handoff dla kolejnej osoby lub AI

[English](ai-handoff.en.md)

## Stan na 2026-10-31

Projekt ma dwa **całkowicie niezależne repozytoria Git** w katalogu `../`:

| Repozytorium | Cel | Stan |
| --- | --- | --- |
| `../firmware` | natywne oprogramowanie pilota | architektura, kontrakt MQTT, specyfikacja UX, trzy makiety, bring-up płytki oraz **działający na sprzęcie firmware: pięć ekranów Pulpitu, Wi-Fi przez ESP32-C6 i sterowanie Z906 wprost po MQTT** |
| `../enclosure` | tylna obudowa do późniejszego użycia baterii | osobny zakres; nie mieszać z firmware |

Docelowy sprzęt to Guition `JC4880P443C_I_W_Y`: ESP32-P4 realizuje aplikację, ekran i dotyk, a ESP32-C6 łączność. Ekran IPS 4,3 cala 480 × 800 w pionie ma być traktowany jako ekran wysokiej jakości — UI ma wyglądać jak gotowy produkt konsumencki, nie panel administracyjny.

Z906 ma już niezależny kontroler ESP32 oraz integrację MQTT/Home Assistant. Pilot nie zastępuje tego kontrolera i nie komunikuje się z nim po UART ani IR — rozmawia z nim po MQTT, wprost na jego tematach (ADR 0006).

## Priorytety i granice

1. Szybkość interfejsu ma pierwszeństwo. Docelowy stos to ESP-IDF + C++17 + LVGL 9, bez warstwy webowej na urządzeniu.
2. Wybudzenie po dotyku może trwać około 2–3 s, ale po nim pilot ma pokazać **nowy, potwierdzony** stan TV i Z906. Nie wolno przedstawiać starego stanu z pamięci jako aktualnego.
3. Nie zapisuj w repozytorium tokenów, haseł, nazw prywatnej sieci ani adresów infrastruktury.
4. Nie wgrywaj firmware'u, nie kasuj flasha i nie zmieniaj okablowania bez wyraźnej zgody użytkownika.
5. Piny, timingi i sterowniki sprzętu trzeba potwierdzić na konkretnym egzemplarzu — nie kopiować ich z podobnych płytek Guition.

## Aktualna kopia firmware'u fabrycznego / zastanego

Przed rozpoczęciem pracy nad UI wykonano pełny, surowy backup 16 MB flasha. Plik jest celowo ignorowany przez Git:

```text
backups/guition-esp32p4-before-ui-2026-10-29.bin
SHA-256: 3c8e61ccdbfb659ecf0c3b6d08eab3c6a250f1ffe43a5d67ee0eaeda5dd10366
```

Backup został tylko odczytany i zweryfikowany skrótem; nie wykonano żadnego zapisu do płytki. Przed ewentualnym przywracaniem trzeba osobno potwierdzić port, rozmiar flasha i zgodę użytkownika.

## Zatwierdzony interfejs v1

Pełna specyfikacja znajduje się w [`../ux/tv-and-z906-ui.md`](../ux/tv-and-z906-ui.md). Najważniejsze ustalenia:

- Są dwa podstawowe widoki: `TV` i `GŁOŚNIKI` (Logitech Z906).
- Na obu jest stale dostępny dolny pasek z tymi dwiema zakładkami. Pasek dochodzi do dolnej krawędzi obudowy ekranu, ale od kontrolek nad nim oddziela go niewielki, celowy odstęp.
- Widok Z906 musi pozostać zaakceptowanym układem **Soundstage**: pierścień głośności, źródła, trzy pionowe suwaki Center/Subwoofer/Tył, wybór efektu oraz informacyjny `Decode 5.1`. Nie wolno zastępować go uproszczoną kartą statusową.
- Widok TV ma zawierać: zasilanie, widoczne źródła, widoczne tryby obrazu, cztery skróty kanałów oraz centralny D-pad z `OK`; nie pokazuje fioletowych etykiet aktualnego źródła i trybu ponad kontrolkami.
- Skróty kanałów są na tym etapie elementem wizualnym: Polsat, TVN, TVP 1 i TVP 2. Docelowo każdy wybiera źródło TV i konkretny kanał przez bridge po stronie Home Assistant.
- Telewizor idzie wprost przez Glasshouse na TV (ADR 0008, `docs/integrations/tv.md`): stan i przyciski po MQTT, skróty kanałów kontraktem v1 przez bridge HA.

Interaktywna makieta robocza tego układu jest w repozytorium:

```text
docs/ux/mockup/lg-tv-remote-ui.html
```

Oprócz ekranów TV i Z906 pokazuje stany z ADR 0005: łączenie po wybudzeniu, błąd połączenia z `Ponów`, oczekiwanie na snapshot po akcji, brak potwierdzenia oraz niedostępne TV/Z906 (panel „Symulacja stanów” pod pilotem). To materiał projektowy, a nie kod do skopiowania na ESP32. Właściwa implementacja ma odwzorować go natywnie w LVGL.

## Co jest już potwierdzone

- Model encji Z906 i zakresy wejść opisano w `docs/integrations/z906.md`.
- `Decode 5.1` to informacja z `binary_sensor`, nigdy przełącznik.
- Wszystkie suwaki Z906 mają zakres 0–100 i krok 2; Center, Subwoofer i Tył są fizycznie przy swoich suwakach.
- Tryb obrazu TV jest stanem dynamicznym zależnym od materiału; wartości i opcje muszą pochodzić z bieżącego snapshotu.
- Po wybudzeniu obowiązuje ADR 0005 i `remote.sync.request`.

## Co nadal nie jest potwierdzone

- Zachowanie łącza Wi-Fi przy dłuższej pracy: ponawianie po zerwaniu, roaming, zachowanie
  przy słabym sygnale. Sprawdzono wyłącznie połączenie i samoczynny powrót po restarcie.
- Pobór prądu i tryby oszczędzania — z włączonym radiem tym bardziej.
- Ekran TV na prawdziwym telewizorze (ADR 0008, 2026-11-02): firmware z klientem Glasshouse
  się kompiluje, ale nie był jeszcze wgrany. Kanały idą `tv.channel.open` przez bridge HA;
  pakiet z `rest_command` nie był jeszcze wgrany do HA (wymaga wpisów w `secrets.yaml`).
- Włączanie TV z czuwania: Wake-on-LAN na telewizorze jest wyłączony, przycisk zasilania TV
  celowo nic nie robi.
- Sposób zasilania bateryjnego i projekt tylnej obudowy — to późniejszy etap repozytorium `enclosure`.

## Bring-up przeprowadzony

Sprzęt jest potwierdzony: ekran zapala się na pełnych 480 × 800, dotyk GT911 trafia w każdy róg,
brak restartów. Wszystkie wartości, wersje i dwie konieczne poprawki opisuje
[`../hardware/verified-board-map.md`](../hardware/verified-board-map.md) — **zacznij od tego pliku**,
jeśli masz dotykać sprzętu.

Najważniejsze, żeby nie tracić na to godziny powtórnie:

- ESP-IDF **5.5.6**, LVGL **9.2.2**, `esp_lvgl_port` **2.6.0**. Nowszy port nie kompiluje się z tym LVGL.
- Komponent `esp_lcd_st7701` z rejestru daje na tej płytce **czarny ekran**. Działa wyłącznie łatana
  kopia producenta.
- BSP producenta trzeba poprawić w jednym miejscu: nie ustawiać źródła zegara PLL dla PHY w MIPI-DSI,
  bo stała zgodności wysypuje układy od rewizji 3.0 wzwyż. Nasz egzemplarz to rev v3.2.

Projekt bring-up powstał poza repozytorium, jako materiał jednorazowy. Do repozytorium trafiły
wyłącznie potwierdzone wyniki.

## Firmware interfejsu — stan na 2026-10-30

Projekt ESP-IDF stoi w `firmware/` w układzie z `docs/architecture/README.md`: `main/` jako
composition root oraz komponenty `board`, `core`, `domain`, `application`, `ui`. Zależności idą
wyłącznie w dół, `ui → application → domain`.

**Szew jest prawdziwy.** `application/SimulatedSource` wystawia dokładnie ten `DeviceSnapshot`,
który poda później MQTT, pracuje we własnym zadaniu FreeRTOS i rozmawia z zadaniem UI dwiema
kolejkami. Dotknięcie wkłada polecenie do kolejki i wraca; zaznaczenie przesuwa dopiero snapshot.
Podmiana symulacji na klienta MQTT dotyka `main.cpp` i nowego komponentu `transport`, nie ekranów.

Potwierdzone na sprzęcie: pięć ekranów przełącza się paskiem zakładek, tarcza głośności i kolumny
kanałów zmieniają wartość względnie (nie skaczą do palca), panel symulacji na ekranie `WIĘCEJ`
odpala stany z ADR 0005. Start trwa 1,9 s, brak restartów i watchdoga.

Narzędzia w `tools/`: `gen-fonts.sh` generuje mapy bitowe Ubuntu przez `lv_font_conv`,
`gen-assets.sh` przerabia ikony zakładek i logotypy z `docs/ux/mockup-pulpit/` na obrazy
LVGL. Oba są odtwarzalne — podmiana kroju albo ikony to ponowne uruchomienie skryptu.

### Łączność — działa

ESP32-C6 odpowiada przez SDIO bez żadnej ingerencji w jego flash: fabryczne firmware 2.3.2
dogaduje się z hostem `esp_hosted` 3.0.10. Szczegóły, piny i dwie pułapki API są
w [`../hardware/verified-board-map.md`](../hardware/verified-board-map.md).

Powstały dwa komponenty i jeden kontroler:

- `transport/` — `WifiService`: skan, łączenie, rozłączanie we własnym zadaniu, wyniki kolejką.
- `storage/` — ustawienia w NVS. **Dane dostępowe zapisujemy dopiero po potwierdzonym
  połączeniu**, żeby nie utrwalać złego hasła. Hasło i nazwa sieci nie trafiają do logu.
- `application/NetworkController` — spina jedno z drugim, przy starcie łączy się z zapamiętaną
  siecią i wraca na nią po zerwaniu (próba co 15 s). Interfejs nie zna ani `transport`, ani NVS.
  Decyzja o ponowieniu należy do kontrolera: usługa Wi-Fi tylko melduje, co się stało.

**Pasek statusu** (usunięty 2026-11-03, patrz „Więcej miejsca na treść”). Nagłówek dzielił się
na pasek statusu (24 px: godzina po lewej, stan łącza po prawej) i wiersz tożsamości (64 px). Suma zostaje równa 88 px, bo strefa treści jest
wypełniona co do piksela — ekran TV zużywa 580 z 588 px i nie odda ani jednego. Wiersz
tożsamości musi mieć równe 64 px: tyle ma przycisk zasilania i przy mniejszym LVGL przycina
mu zaokrąglenia.

**Wygaszanie ekranu.** Po 30 s bez dotyku gaśnie podświetlenie; panel pracuje dalej, więc powrót
jest natychmiastowy. Tuż przed zgaszeniem wchodzi przezroczysta warstwa na `lv_layer_sys`, żeby
pierwszy dotyk tylko wybudził i nie nacisnął kontrolki pod palcem — inaczej budzenie w ciemno
wyłączałoby telewizor. Czas liczy `lv_display_get_inactive_time()`, więc resetuje go każde
zdarzenie dotyku, nie tylko kliknięcie.

To **wyłącznie wygaszenie ekranu**, nie oszczędzanie energii: układ pracuje normalnie, Wi-Fi stoi.
Tryb z ADR 0004 to osobny temat i wymaga sprawdzenia, czy dotyk tej płytki pewnie wybudza wybrany
tryb uśpienia.

**Wybudzenie musi odświeżyć stany wszystkich kontrolek.** Dziś ekran wraca z tym, co było przed
zgaszeniem, i to jest tymczasowe — ADR 0005 zabrania pokazywania zapamiętanego stanu jako
aktualnego, a przez pół minuty ciemnego ekranu telewizor albo wzmacniacz mógł zmienić stan
z pilota, z aplikacji lub z automatyzacji. Docelowo wybudzenie wysyła `remote.sync.request`
i trzyma nakładkę do czasu snapshotu o nowej rewizji, dokładnie tak jak wybudzenie z uśpienia.

Hak jest już na miejscu: symulowane źródło obsługuje `IntentKind::RequestSync` i odtwarza pełną
sekwencję. Brakuje wyłącznie transportu po drugiej stronie, więc to zadanie domyka się razem
z klientem MQTT. Miejsce do zmiany jest oznaczone w `components/ui/ui.cpp` przy `wake()`.

**Czas pobieramy raz.** `transport/time_sync` strzela do SNTP po połączeniu, a gdy czas przyjdzie,
zatrzymuje klienta. Stałe odpytywanie wybudzałoby pilota przy pracy na baterii. Ponowienie
następuje dopiero po dobie, bo oscylator układu dryfuje i bez korekty godzina ucieka o minuty.
Konfiguracja z serwerem czasu z DHCP wymaga osobnego slotu i przy domyślnej liście kończy się
błędem argumentu — używamy jednego serwera.

W interfejsie doszła **podstrona drugiego poziomu** (`widgets::make_page`): pełnoekranowa nakładka
z powrotem w nagłówku. Pasek zakładek ma twarde pięć slotów, więc wszystko głębsze idzie tędy.
Pierwsza taka podstrona to ekran Wi-Fi: stan, zapamiętana sieć, skanowanie, lista z siłą sygnału
oraz klawiatura ekranowa na hasło.

### Świadome odstępstwa, do zamknięcia ADR-em albo poprawką

- **Ikony funkcyjne** (strzałki, zasilanie, głośność) są wbudowanymi symbolami LVGL z kroju
  Montserrat, bo ikon lucide z makiety nie ma lokalnie. Ikony zakładek są już właściwe — pixel art
  z `pixel-icons.js`. Docelowo cały zestaw powinien przejść tą samą drogą co zakładki.
- **Sekcja `appletv` weszła do kontraktu v1** addytywnie, przez
  [ADR 0007](../architecture/adr/0007-apple-tv-via-home-assistant.md).
- **Klawiatura Apple TV wysyła tekst po „Zapisz”**, a nie na bieżąco przy każdym znaku.
  Wspólny edytor nie zgłasza zmian w trakcie pisania; jeśli to przeszkadza, trzeba dodać mu
  wywołanie zwrotne zmiany z ograniczeniem częstotliwości.
- **Każdy snapshot zamyka wszystkie oczekiwania**, więc snapshot Z906 może zdjąć obwódkę
  oczekiwania z kafelka Apple TV, zanim HA potwierdzi uruchomienie. Wizualnie drobne, logicznie
  bezpieczne — zaznaczenie i tak przesuwa dopiero `app_id` z HA.
- **Skróty kanałów, sceny i skróty ekranu `DOM`** są poleceniami chwilowymi: nie mają stanu do
  potwierdzenia, bo nie ma ich w kontrakcie.
- **Plakaty są wbudowane** zamiast pobierane po HTTP. Biblioteka Emby wymaga osobnego ADR-u.

## Stan łączności

**Wi-Fi stoi.** `esp_wifi_remote` + `esp_hosted` przez SDIO do ESP32-C6, piny potwierdzone na
tym egzemplarzu i opisane w [`../hardware/verified-board-map.md`](../hardware/verified-board-map.md).
Sieć wybiera się na ekranie `WIĘCEJ`, dane idą do NVS.

**Dźwięk idzie wprost do kontrolera Z906** po MQTT, z pominięciem Home Assistanta — powód
i tabela tematów w [ADR 0006](../architecture/adr/0006-direct-z906-topics.md). Klient siedzi
w `transport/z906_client.cpp`, przejściówka na `SnapshotSource` w `application/z906_source.cpp`.
Identyfikator kontrolera ma postać `z906_<mac>`, wpisuje się go na urządzeniu i leży wyłącznie
w NVS — w repozytorium go nie ma i nie wolno go tu wpisać.

**Apple TV idzie przez Home Assistanta** ([ADR 0007](../architecture/adr/0007-apple-tv-via-home-assistant.md)).
Klient kontraktu v1 wrócił jako `transport/ha_client.cpp` i czyta wyłącznie sekcję `appletv`.
`application/live_source.cpp` składa go z `Z906Source` w jedno źródło z własną rewizją;
nakładkę łączenia zdejmuje Z906, a brak HA wygasza tylko ekran Apple TV. Klient HA rusza,
gdy na stronie MQTT podano **Identyfikator pilota** (`remote_id` w NVS).

Stan wdrożenia Apple TV:

- integracja `apple_tv` w HA sparowana; encje, bundle id i identyfikator
  wpisu są w bridge'u i w `domain/appletv_apps.hpp`, odczytane z urządzenia — patrz
  [`../integrations/appletv.md`](../integrations/appletv.md);
- 2026-10-31 sprawdzono przez REST API HA na prawdziwym Apple TV: strzałki, `play_media`
  z bundle id, usypianie i budzenie przez `media_player.turn_off/turn_on` (**nie** `remote.*`,
  które przełącza tylko stan w HA) oraz `set_keyboard_text` z polskimi znakami;
- ukryte gesty pilota Siri są na pilocie: Home ×2 (przełącznik aplikacji), przytrzymanie Home
  (Centrum sterowania), przytrzymanie OK (menu kontekstowe); w HA sprawdzone, na pilocie nie;
  zwykłe stuknięcie Home wychodzi ~300 ms później, bo czeka na ewentualne drugie;
- atrybut `app_id` w HA bywa nieaktualny, więc podświetlenie aktywnego kafelka może się spóźniać;
- pakiet bridge'a wgrany do HA 2026-10-31 (`packages/multimedia_remote.yaml`, kopia
  `configuration.yaml.before-multimedia-remote.*`), `check_config` czysty, po restarcie licznik
  rewizji rośnie, sekcja `appletv` zawiera trzy aplikacje; identyfikator pilota `living-room-remote`;
- firmware wgrany na pilota 2026-10-31. Pierwsza wersja wpadała w pętlę restartów: przepełniał
  się stos zadania LVGL, bo `LiveSource::poll` trzymał na nim kopie snapshotu. Poprawione
  (bufor HA jako pole klasy, wynik prosto w `out`) i stos LVGL podniesiony do 12 kB. Po
  poprawce start jest czysty, a Z906 łączy się i odbiera stan;
- identyfikator `living-room-remote` wpisany na pilocie; sterowanie Apple TV z pilota działa;
- opóźnienie klawiszy: esp-mqtt wysyłał kolejkę (`esp_mqtt_client_enqueue`) co 1000 ms —
  w śladach HA klawisze przychodziły równo co sekundę, a HA wykonywał je w 10–18 ms. Poprawka:
  `CONFIG_MQTT_POLL_READ_TIMEOUT_MS=20` w `sdkconfig.defaults`, nawigacja z QoS 0, klient HA
  ponawia połączenie co 1,5 s jak Z906 (wcześniej łączył się 8 s po dźwięku);
- klawiatura ekranowa (po commicie 6ce939c, niezacommitowane): własna macierz zamiast
  lv_keyboard, dwa style — siatka jak na Apple TV (domyślnie) i QWERTY jak na telefonie,
  przełącznik w nagłówku (NVS `kb_qwerty`); tryb „Na żywo” wysyła tekst na Apple TV co 250 ms
  w trakcie pisania (NVS `kb_live`, domyślnie włączony). Bridge nie publikuje snapshotu po
  `appletv.keyboard.*`; wgrany do HA;
- ekran Apple TV: gładzik 360 px, pod nim Menu i Home, na dole kafelki aplikacji.
  Biblioteka „Nowe filmy / Nowe seriale” (lista, plakaty, `AtvPlay`) usunięta — wróci
  zaprojektowana od nowa z kontraktu, po ADR-ze dla Emby;
- w trybie „Dane udawane” cały ekran działa: OK w YouTube wysuwa klawiaturę, Menu ją chowa.

**Wybudzenie odświeża stan** zgodnie z [ADR 0005](../architecture/adr/0005-fresh-state-after-wake.md):
dotyk wysyła żądanie synchronizacji i nakładka łączenia zostaje, dopóki nie przyjdzie snapshot
o nowej rewizji. Gaśnie samo podświetlenie — radio zostaje włączone.

**Pierwszy stan jest na ekranie około dziewięciu sekund po starcie**: dwie sekundy na interfejs,
siedem na skojarzenie z Wi-Fi, reszta na brokera. Klient MQTT rusza, zanim jest IP, więc pierwsze
próby padają — tak ma być, ponawia je co 1,5 s.

**Sygnał Wi-Fi w miejscu pilota jest słaby**: od -73 do -86 dBm, podczas gdy kontroler Z906
z tej samej sieci raportuje -57 dBm. Skojarzenie z siecią zajmuje przez to od pięciu do
dwunastu sekund i to ono, a nie broker, decyduje o czasie do pierwszego stanu. Limit
synchronizacji liczy się od ostatniego kroku łączenia właśnie dlatego.

**TV nie ma źródła.** Jego ekran pokazuje brak świeżego stanu i to jest poprawne,
dopóki integracja LG nie jest potwierdzona.

## Plan ustawień i refaktoru

Plan: [`docs/plans/2026-10-31-ustawienia-i-refaktor.md`](../plans/2026-10-31-ustawienia-i-refaktor.md).

- **Etap 0 (zrobiony, sprawdzony na pilocie):** Z906 wysyła przez `enqueue`, flaga trybu prób
  w pamięci podręcznej, atomowe pola klientów, reset rewizji HA po połączeniu.
- **Etap 1 (zrobiony, skompilowany; na pilocie niesprawdzony):**
  - biblioteka filmów usunięta;
  - preferencje jednobajtowe przez `storage::get/set(Pref)` z pamięcią podręczną
    i uchwytem NVS w RAII; nowe ustawienie to wiersz w `kPrefs` i w `Pref`;
    `storage::factory_reset()` czeka na ekran WIĘCEJ;
  - `widgets.cpp` podzielony: podstawowe, `widgets_controls.cpp` (tarcza, poziom),
    `widgets_nav.cpp` (krzyżak, gładzik), `widgets_editor.cpp` (edytor i klawiatura),
    wspólne pomocniki w `widgets_internal.hpp`;
  - czasy zachowania w `core/config.hpp`.
  - Nie zrobione z etapu 1: symulacja za opcją Kconfig, `bind<T>()` dla callbacków.
- **Etap 2, partia pierwsza (zrobiona, sprawdzona na pilocie):**
  - **Ekran:** jasność 20–100 % zapamiętana i przywracana po wybudzeniu
    (`board::set_brightness`/`dim`), wygaszanie 15 s – nigdy, przyciemnianie 10 s przed.
  - **Dźwięk:** limit głośności Z906 wysyłanej z tarczy.
  - **Wi-Fi:** sygnał w dBm, liczba zerwań i nieudanych prób, ostatni powód.
  - **O urządzeniu:** wersja, pamięć, czas pracy, restart i ustawienia fabryczne
    przez `widgets::ask_confirm`.
  - Strony ustawień i lista WIĘCEJ przewijają się w pionie.
  - Domyślne wartości odtwarzają zachowanie sprzed ustawień.
- **Etap 3, partia druga (zrobiona, sprawdzona na pilocie):**
  - **Ekran:** tryb nocny (jasność najwyżej 20 % w wybranych godzinach), ekran po starcie
    (także „ostatni”).
  - **Pokój i czas:** nazwa pokoju w nagłówkach (NVS `room`), strefa czasowa
    (`transport::set_timezone`).
  - **Klawiatura i gładzik:** styl, tekst na żywo, wysuwanie samo albo przyciskiem,
    czułość gładzika (20/26/34 px).
  - Przycisk „Tekst” na ekranie Apple TV otwiera klawiaturę bez pola tekstowego na TV.
  - **Diagnostyka:** IP, łącze z domem, Z906, HA, Apple TV, rewizja stanu.
  - Wolna pamięć wewnętrzna po starcie spadła do ok. 181 kB. Kolejne strony warto
    budować przy pierwszym otwarciu.
- **Następny:** fundament pod TV (etap 4 planu) albo OTA (etap 5, osobny ADR).

## Klimatyzacja — stan na 2026-11-02

Ekran `DOM` ma kafelek klimy (tryb, temperatura docelowa, temperatura w pokoju), a jego
dotknięcie otwiera podstronę `Klimatyzacja`: Cofnij i zasilanie w nagłówku, tarcza
temperatury `widgets::Thermostat` z −/+, tryby pracy, kafelki Moc / Pion / Poziom
z listą wyboru. Projekt: makieta `docs/ux/mockup-pulpit` (sekcja „Ekran Dom i
klimatyzacja” w README). Kod: `ui/screens/screen_climate.cpp`. Skróty Światło, Rolety,
Klimat, Kamera zniknęły z ekranu `DOM`.

- Model: `domain::ClimateState` w snapshocie — identyfikatory HA (`cool`, `medlow`,
  `Full`), temperatury w dziesiątych, zakres i krok z encji. Nazwy po polsku dobiera ekran.
- Sterowania `ClimatePower/Mode/Target/Fan/Swing/SwingH`; temperatura jedzie w
  `UiIntent::number`.
- **Na żywo przez Home Assistanta** ([ADR 0009](../architecture/adr/0009-climate-via-home-assistant.md)):
  sekcja `climate` w snapshocie kontraktu v1 i akcje `climate.*`; bridge czyta
  `climate.living_room_ac`, firmware — `transport/ha_client.cpp` (`climate_from`). Bridge wgrany
  2026-11-03 (kopia `multimedia_remote.yaml.before-climate.20261103-001829`,
  `check_config` czysty, przeładowane tylko skrypty i automatyzacje, bez restartu HA).
  Pilot odebrał świeży stan klimy z HA (`klima dostepna off 200`). **Poleceń z pilota do
  klimy jeszcze nie sprawdzono** — każde zmienia prawdziwe urządzenie. Do sprawdzenia też,
  czy Midea zgłasza nową wartość w 0,4 s, po których bridge publikuje snapshot; jeśli nie,
  zaznaczenie na pilocie na chwilę wróci do starej wartości.
- Wgrane na pilota 2026-11-03. **Pierwsza wersja wpadała w pętlę restartów**: snapshot
  urósł o ok. 1,1 kB, a `Z906Event` niósł cały `DeviceSnapshot` — `emit_snapshot()`
  przepełniał 6 kB stosu `mqtt_task` ok. 9 s po starcie, a kolejka klienta Z906
  (12 miejsc) zjadała ok. 43 kB RAM-u wewnętrznego. Poprawka: `Z906Event` niesie tylko
  `Z906State` i rewizję, snapshot skleja `Z906Source`/`LiveSource`. Zasada na przyszłość:
  **transport wysyła wyłącznie stan swojego urządzenia**, nigdy cały snapshot.
- Po poprawce: start czysty, 60 s bez restartu, Z906 i HA łączą się. Wolna pamięć
  wewnętrzna po zbudowaniu ekranów: **124 kB** (przed poprawką 88 kB), z klimą na żywo
  (zdarzenie HA niesie też `ClimateState`) — **117 kB**.
  Ostatni zapisany pomiar sprzed klienta TV i klimy to ok. 181 kB — różnicy nie rozbito
  na części. Klima na pilocie działa na żywo (sprawdzone przez użytkownika 2026-11-03).
- Kolejny krok (uzgodniony): przegląd kopii snapshotu i kolejek w pamięci wewnętrznej
  przed dokładaniem ekranów.

## Symulacja usunięta — 2026-11-03

Tryb „Dane udawane” zniknął z firmware na prośbę użytkownika (nie był używany):
`SimulatedSource`, `SimulationControl`, panel „Symulacja” w WIĘCEJ, wiersz „Źródło stanu”
na stronie MQTT i preferencja `Pref::Simulation` (klucz `sim` może zostać w NVS, nikt go
nie czyta). Pilot ma jedno źródło: `LiveSource`. Stany z ADR 0005 (łączenie, błąd, brak
świeżego stanu) da się teraz zobaczyć tylko na prawdziwym łączu — albo w makiecie HTML,
której panel symulacji zostaje. Wgrane 2026-11-03: wolna pamięć wewnętrzna po zbudowaniu
ekranów wzrosła ze 117 do **153 kB** (kolejka 4 × snapshot, zadanie z 6 kB stosu, kopie
stanu i panel).

## Ekran startowy — projekt, 2026-11-03

Makieta `docs/ux/mockup-pulpit` (sekcja „Ekran startowy” w README): po starcie i po każdym
wybudzeniu — i tylko wtedy — pełny ekran z zegarem, datą i czterema dużymi kafelkami
(Telewizor, Apple TV, Głośniki, Dom) z ikonami pixel art 4× i jednym zdaniem stanu.
Dotknięcie kafelka zamyka go do następnego wybudzenia. Pasek zakładek bez zmian, z `WIĘCEJ`
(decyzja użytkownika: bez zakładki START).

**Port do LVGL (2026-11-03, [ADR 0010](../architecture/adr/0010-start-screen-after-wake.md)):**
`ui/screens/screen_start.cpp`, warstwa na `lv_layer_sys` nad podstronami. `show_start(true)`
woła start (`ui::start`) i wybudzenie (`wake()` w `ui.cpp`); zamyka wtedy podstrony, edytor
i pytanie potwierdzenia, żeby po wyborze kafelka nie przykryły ekranu urządzenia. Nakładka
łączenia (`render_shell`) jest ukryta, dopóki start jest otwarty. Ikony 96 px to
`icon_*_large` z `tools/gen_assets.py`, zegar — `font_b64num` (doszedł dwukropek).
Kropka „łączenie…” jest statyczna, bez pulsowania z makiety. Wgrane 2026-11-03: start
czysty, Z906 i HA łączą się, 153 kB wolnej pamięci wewnętrznej. Wygląd i dotyk kafelków
ocenia użytkownik na urządzeniu.

Najważniejsza zmiana zachowania: ekran startowy nie pokazuje stanu sterowania, więc wychodzi
**od razu po wybudzeniu**, a synchronizacja (ADR 0005) dzieje się w tle — kafelki mówią
„łączenie…”, dopóki snapshot nie przyjdzie. Nakładka łączenia pojawia się tylko wtedy, gdy
ktoś wejdzie na ekran urządzenia, zanim stan dotrze.

Ustawienie „Ekran po starcie” (Więcej → Ekran) **usunięte** 2026-11-03 razem z
`Pref::StartScreen` i `Pref::LastScreen` — po wybudzeniu zawsze jest ekran startowy,
a pod nim czeka TV.
- Segmenty trybów są tekstowe — brak ikon funkcyjnych (patrz „Świadome odstępstwa”).
  Lista mocy to podstrona z wierszami jak lista źródeł TV, nie okno od dołu z makiety.

Mapa kodu interfejsu i przepis na budowanie: [`../architecture/ui-architecture.md`](../architecture/ui-architecture.md).

## Pamięć — pomiar i przeniesienie do PSRAM, 2026-11-03

Log startu ma teraz linie `mem:` (`core/memory`): wolna pamięć wewnętrzna, największy
wolny blok, minimum od startu i PSRAM po każdym etapie, a po 30 s zapas stosu każdego
zadania (`CONFIG_FREERTOS_USE_TRACE_FACILITY`).

| Etap | Przed | Po |
| --- | --- | --- |
| start (po .bss) | 449 kB | 490 kB |
| powłoka i ekrany | 246 kB | 473 kB |
| sieć i klienci | 150 kB | 385 kB |
| po 30 s (min od startu) | 160 kB (131) | 395 kB (368) |
| po 30 s, po punktach 2–7 | — | **439 kB (416)** |

Co zrobiono:

- **Alokator LVGL w PSRAM** (`components/board/lv_mem_psram.c`, `CONFIG_LV_USE_CUSTOM_MALLOC`).
  `SPIRAM_MALLOC_ALWAYSINTERNAL=4096` trzymał każdy obiekt LVGL w pamięci wewnętrznej —
  pięć ekranów brało 173 kB. Teraz 0 kB; budowa interfejsu trwa jak przedtem (~165 ms).
  `board/CMakeLists.txt` wymusza dołączenie pliku przez `-u lv_mem_init` (kolejność
  linkowania).
- **Duże obiekty w PSRAM:** `LiveSource`, `AppController`, `WifiService`,
  `NetworkController` (`main.cpp`) i `ui::Context` (`ui.cpp`) powstają przez
  `core::memory::make_in_psram` na początku startu zamiast w `.bss` (+41 kB). Nie
  `EXT_RAM_BSS_ATTR`: obiekt inicjalizowany już przy kompilacji straciłby tam wartości.

Płynność rysowania z obiektami w PSRAM użytkownik sprawdził — bez zmian.

Dalej tego samego dnia:

- **Jedno połączenie MQTT** (`transport/mqtt_bus.*`) dla Z906, HA i Glasshouse zamiast
  trzech. Klienci podpinają handlery zdarzeń przed `bus.start()` i sami odsiewają swoje
  tematy (także kawałki długich wiadomości: `assembly_mine_`). Testament kontraktu v1
  ustawia magistrala. Identyfikator: `pilot-<remote_id>` (albo `pilot-<z906_id>`).
- **Wysyłka własnym zadaniem** `mqtt_tx` (stos w PSRAM) przez `esp_mqtt_client_publish`,
  nie `enqueue`. Na wspólnym połączeniu `MQTT_POLL_READ_TIMEOUT_MS=20` zrywał je co ~1,5 s
  (`Network timeout while reading MQTT message`): esp-mqtt zrywa połączenie, gdy przez
  jeden obieg pętli w środku wiadomości nie dojdzie ani bajt, a obieg trwa tyle, co ten
  limit. Limit wrócił do 1000 ms; klawisze nie czekają na obieg, bo nie idą kolejką
  biblioteki. 75 s bez rozłączenia. **Opóźnienie klawiszy Apple TV do sprawdzenia
  dotykiem.**
- **Menu kanałów:** wiersz to przycisk i dwie etykiety (3 obiekty LVGL zamiast 7).
- **Log przez MQTT domyślnie wyłączony** (`CONFIG_PILOT_REMOTE_LOG`, osobne połączenie);
  `tools/pilot_log.py` działa dopiero po włączeniu go w menuconfig. Jego zadanie ma stos
  w PSRAM.
- **Stosy w PSRAM tylko dla zadań bez flasha:** `wifi_svc` zostaje w pamięci wewnętrznej,
  bo przy starcie woła `nvs_flash_init` (przy operacji na flashu PSRAM jest niedostępny).
- **Listy wejść i efektów Z906 poza snapshotem** (`domain/z906_options.hpp`): są stałe
  dla modelu, a jechały w każdej kopii stanu i 12 miejscach kolejki klienta.

Niezrobione z review: budowanie ekranów w tle po pokazaniu startu, zmniejszenie stosu LVGL
(zużywa ~4 z 12 kB, ale pomiar był bez otwierania ekranów — najpierw zmierzyć po pełnym
obejściu interfejsu), zapamiętanie BSSID i kanału Wi-Fi dla szybszego łączenia.

## Ikony pixel art 48 × 48 — 2026-11-03

Ikony zakładek i kafelków startu przerysowane na siatce 48 × 48 z cieniowaniem i detalami
(użytkownik: dawne 24 × 24 miały za mało szczegółów). Źródło: PNG z `tools/draw_icons.py`
w `docs/ux/mockup-pulpit/assets/pixel/`; `tools/gen_assets.py` robi z nich `I8` (dawniej `I4`
z siatek w `pixel-icons.js`). Wgrane; start czysty. Wygląd na pilocie ocenia użytkownik.

## Ekran dźwięku — 2026-11-03

- Przyciski −/+ po bokach tarczy głośności, krok 1, przytrzymanie powtarza
  (`widgets::make_step_button`). Kontroler przyjmuje nieparzyste wartości (w HA było 23),
  choć encja deklaruje krok 2 — do sprawdzenia uchem, czy każdy krok zmienia poziom.
- Wejście i efekt w jednym rzędzie jako segmenty z bieżącą wartością
  (`widgets::make_value_segment`, wspólny z ekranem TV), wybór na podstronie z listą.
- Decode 5.1 przeniesiony do nagłówka (pastylka obok zasilania, tylko na ekranie dźwięku
  i tylko ze świeżego stanu). Odzyskane miejsce: tarcza 208 → 248 px (odczyt 64 px),
  kolumny kanałów 80 → 140 px (pełny zakres: 160 px ruchu palca).

Później tego dnia (wzór: tarcza termostatu w HA, zrzut od użytkownika):
- Tarcza głośności 248 → 296 px, łuk 18 → 22 px, uchwyt na końcu wypełnienia (białe koło
  z obwódką w kolorze akcentu). −/+ (64 px) przeszły z boków do otwarcia łuku u dołu,
  wyciszenie (52 px) nad liczbę (`Dial::readout`).
- Oczekiwanie na potwierdzenie głośności nie barwi już liczby na pomarańczowo (na prośbę
  użytkownika); czerwień zostaje przy błędzie. W makiecie tarcza nie pulsuje obwódką.
- Klimatyzacja: −/+ termostatu też w otwarciu łuku (64 px), skala 16°/31° odsunięta
  pod końce łuku, `kThermoHeight` = 300.
- Ekran `DOM` bez wiersza „Snapshot · rewizja N” — rewizja jest w Diagnostyce.
- Pasek zakładek: górny odstęp 8 → 4 px (4 + 72 + 4), bo nad zaznaczoną zakładką było
  wyraźnie więcej miejsca niż pod nią.

## Pogoda na ekranie startowym — 2026-11-03

[ADR 0011](../architecture/adr/0011-weather-on-start-screen.md): bridge publikuje
`weather.forecast_home` na temacie retained `multimedia_remote/v1/<id>/weather`
(`condition`, `temperature`, `night` z `sun.sun`) przy zmianie, wschodzie i zachodzie słońca
oraz po starcie HA. Pilot (`HaClient::take_weather` → `LiveSource::weather_`) trzyma ostatnią
w pamięci — to treść, nie stan, więc ekran startowy pokazuje ją od razu po wybudzeniu, a po
6 h bez aktualizacji chowa. Ikony 32 × 32 → 64 px (`weather_*.png`, `icon_weather_*`).

- `night` jest potrzebne, bo met.no w HA daje `partlycloudy` także w nocy (użytkownik
  zobaczył słońce o 2:16); nocą pilot rysuje księżyc za chmurą i pisze „częściowe zachmurzenie”.
- Bridge wgrany (kopie `multimedia_remote.yaml.before-weather.20261103-021541`
  i `.before-weather-night.20261103-021757`), `check_config` czysty, przeładowane skrypty
  i automatyzacje. Pilot odebrał pogodę (`ha: pogoda: partlycloudy 55`).
- Nowy plik `.c` w `ui/assets` wymaga `idf.py reconfigure` (CMake `file(GLOB)`), inaczej
  linker nie znajdzie obrazu.

## Więcej miejsca na treść — 2026-11-03

Na prośbę użytkownika („to pilot, nie telefon”): nagłówek bez paska statusu (godzina i Wi-Fi
są w ustawieniach), `kTopbar` 88 → 64; zakładki z samym podpisem, bez ikon, `kTabbar`
96 → 76 (cel dotyku 68 px). Strefa treści 588 → 632 px, rozdana tak: krzyżak TV 236 → 256 px,
kafelki kanałów i aplikacji Apple TV 88 → 100 px, gładzik 360 → 400 px, kolumny kanałów Z906
140 → 170 px, sceny Dom 104 → 120 px. Wgrane, start czysty; układ do oceny na urządzeniu.

Poprawki tego samego dnia: ikony zakładek wróciły (bez nich było dziwnie, a zysk mały) —
`kTabbar` znów 96; nad wierszem nagłówka 5 px (`kTopbarPad`), bo zasilanie dotykało
krawędzi; `kTopbar` = 69. Na koniec dnia podpisy zakładek zniknęły — same ikony 48 px,
`kTabbar` = 80 (8 + 68 + 4; od 2026-11-03 wieczorem 4 + 72 + 4). Treść: 623 px. Kafelki aplikacji Apple TV są nad gładzikiem,
w rozmiarze skrótów kanałów TV (80 × 100), wyśrodkowane.

Później tego dnia skróty kanałów TV i kafelki aplikacji Apple TV zeszły do kwadratu
80 × 80: przy logo do 38 px wysokie 100 px wyglądało nienaturalnie. Zwolnione 20 px bierze
krzyżak (TV) i odstęp wokół gładzika (Apple TV).

## Log pilota przez MQTT (na czas rozwoju)

`CONFIG_PILOT_REMOTE_LOG` (menu „Pilot”, domyślnie włączone) kopiuje każdą linię ESP_LOG
do tematu `pilot_debug/<identyfikator pilota>/log` na brokerze z ustawień MQTT. Linie sprzed
połączenia czekają w buforze 32 kB w PSRAM, więc widać też start. Klient logu rusza dopiero,
gdy usługa Wi-Fi podniesie stos TCP/IP — wcześniejszy start kończył się asercją lwIP
(„Invalid mbox”) i pętlą restartów.

Czytanie przez websocket Home Assistanta (broker wymaga logowania, HA ma dostęp):

```bash
HA_URL=http://<adres-ha>:8123 HA_TOKEN=... uvx --with websockets python tools/pilot_log.py + 60
```

Log nie zawiera sekretów: nazwy sieci, hasła i adresy nie są logowane, tag `eh_wifi`
(ESP-Hosted wypisywał nazwę sieci) jest ograniczony do ostrzeżeń. Przed wydaniem wyłączyć.

Klient HA loguje każdy przyjęty snapshot (rewizja, dostępność Apple TV, liczba aplikacji)
i utratę połączenia z brokerem — po to, by odróżnić „HA milczy” od „HA mówi: niedostępne”.

## Konsola USB: zrzuty ekranu i zdalny dotyk (na czas rozwoju)

`CONFIG_PILOT_USB_CONSOLE` (menu „Pilot”, domyślnie włączone) instaluje sterownik
USB-Serial-JTAG i słucha na nim komend `shot`, `tap x y` oraz `drag x1 y1 x2 y2 ms`.
`shot` kopiuje bufor ramki panelu DPI (to, co widać, razem z nakładkami z `lv_layer_top`
i `lv_layer_sys`) i odsyła go w base64; na czas wysyłki ESP_LOG milknie. Dotyk idzie osobnym
wejściem LVGL obok GT911. Obsługa z komputera:

```bash
uvx --with pyserial --with pillow python tools/remote.py shot zrzut.png
uvx --with pyserial --with pillow python tools/remote.py tour docs/screenshots/
```

Około 2,5 s na zrzut, bez restartu pilota. `tour` dotyka wyłącznie paska zakładek — zdalny
dotyk w kontrolkę naprawdę steruje domem. Ekran ustawień pokazuje nazwę sieci Wi-Fi: przed
publikacją zrzutu trzeba ją zamaskować.

Pułapka: macOS podnosi DTR i RTS przy otwarciu portu, a USB-Serial-JTAG resetuje układ przy
RTS=1 i DTR=0 — RTS trzeba opuścić przed DTR (tak robi skrypt). Zwykłe otwarcie portu
z `dtr=False, rts=False` ustawionymi przed `open()` restartuje pilota, a zrzut łapie wtedy
ramkę z rozruchu.

## Następny bezpieczny krok

1. Ekran WIĘCEJ według planu powyżej (etap 2).
2. TV: integracja LG w Home Assistant i sekcja `tv` — klient HA już jest, wystarczy ją czytać.
3. Oszczędzanie energii naprawdę, a nie samo gaszenie podświetlenia: wymaga potwierdzenia
   sprzętowej ścieżki wybudzenia dotykiem i pogodzenia się z dłuższym wybudzaniem.
4. Dopiero potem tematy wymagające ADR-ów: biblioteka Emby, plakaty po HTTP.

Przed kodem sprzętowym lub wgraniem czegokolwiek należy:

1. sprawdzić `git status --short --branch` w obu repozytoriach;
2. przeczytać `AGENTS.md`, `docs/architecture/README.md`, `docs/architecture/mqtt-contract.md` oraz `docs/hardware/bring-up.md`;
3. uzyskać wyraźne pozwolenie na zapis do płytki;
4. po każdym istotnym etapie uaktualnić ten dokument: data, commit, co potwierdzono, co pozostało niepotwierdzone i kolejny bezpieczny krok.
