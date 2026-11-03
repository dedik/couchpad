# Zweryfikowana mapa płytki

[English](verified-board-map.en.md)

Wartości potwierdzone **na tym egzemplarzu**, nie przepisane z podobnych modeli. Procedura i
kryteria pochodzą z [`bring-up.md`](bring-up.md).

- **Data:** 2026-10-29 (sprzęt), 2026-10-30 (firmware interfejsu)
- **Płytka:** Guition JC4880P443C_I_W_Y
- **Sposób testu:** własny program `pilot_bringup` wgrany przez USB; odczyt logu startowego z portu
  szeregowego oraz oględziny ekranu i dotyku. 2026-10-30 to samo dla właściwego firmware'u
  z repozytorium (`idf.py flash`, pięć ekranów Pulpitu na danych symulowanych)

## Potwierdzone na tym egzemplarzu

| Parametr | Wartość | Skąd wiadomo |
| --- | --- | --- |
| Układ | ESP32-P4 **rev v3.2**, 2 rdzenie + LP, 400 MHz | `esptool chip-id` oraz log startowy |
| Flash | 16 MB, producent 0x68, układ 0x4018, tryb QIO 80 MHz | `esptool flash-id` |
| Kontroler flash | Boya — IDF używa sterownika ogólnego i ostrzega o tym | log startowy |
| PSRAM | **32 MB**, tryb HEX, 200 MHz; wolne ~33,0 MB | log startowy |
| Panel | 480 × 800, RGB565, MIPI-DSI, 2 linie danych | log startowy + obraz na ekranie |
| Sterownik panelu | ST7701 z **łataną** sekwencją producenta | kod BSP + działający obraz |
| Dotyk | **GT911**, I2C, odpowiada: `TouchPad_ID 0x39,0x31,0x31` („911"), konfiguracja v250 | log startowy |
| Pełna powierzchnia | ramka widoczna na wszystkich czterech krawędziach | oględziny |
| Dotyk w rogach | wszystkie cztery narożniki trafione, odczyt do `X 459 Y 774` | oględziny |
| Stabilność | brak restartów i artefaktów po poprawce zegara | log startowy |
| Kolory | zgodne — bursztyn jest bursztynem, tło prawie czarne; kolejność kanałów RGB poprawna | oględziny na żywo |

## Piny użyte i działające

Pochodzą z BSP producenta dla tego modelu; ekran i dotyk na nich ruszyły.

| Funkcja | GPIO |
| --- | --- |
| I2C SDA / SCL (dotyk, kodek) | 7 / 8 |
| Podświetlenie (LEDC) | 23 |
| Reset panelu | 5 |
| Reset i przerwanie dotyku | niepodłączone |
| I2S MCLK / BCLK / LRCK / DOUT / DIN | 13 / 12 / 10 / 9 / 48 |
| Włączenie wzmacniacza | 11 |
| microSD D0–D3 / CMD / CLK | 39, 40, 41, 42 / 44 / 43 |
| Zasilanie PHY MIPI-DSI | wewnętrzny LDO, kanał 3, 2500 mV |
| Zasilanie microSD | wewnętrzny LDO, kanał 4 |

Parametry obrazu z łatanego komponentu: zegar DPI 34 MHz, lane bit rate 750 Mbps, ganki
42/12/42 w poziomie i 8/2/166 w pionie.

## Przypięte wersje

| | |
| --- | --- |
| ESP-IDF | **v5.5.6** |
| LVGL | **9.2.2** |
| esp_lvgl_port | **2.6.0** |
| esp_lcd_touch_gt911 | **1.1.3** |
| esp_lcd_st7701 | **1.1.3**, kopia łatana przez producenta, trzymana w repozytorium |

Tych wersji **nie wolno luzować bez testu na sprzęcie**. `esp_lvgl_port` 2.9 używa stałej
`LV_COLOR_FORMAT_RGB565_SWAPPED` dodanej dopiero w LVGL 9.3 i z LVGL 9.2.2 nie kompiluje się wcale.

## Poprawki konieczne na tym egzemplarzu

**Źródło zegara PLL dla PHY w MIPI-DSI.** BSP producenta ustawia
`.phy_clk_src = MIPI_DSI_PHY_CLK_SRC_DEFAULT`, czyli stałą zgodności oznaczającą `PLL_F20M`. Dla
układów od rewizji 3.0 wzwyż ESP-IDF kompiluje wariant HAL znający wyłącznie XTAL i cztery PLL-e,
więc `PLL_F20M` wpada w `default: abort()`. Efekt: pętla restartów, zanim ekran zdąży się zapalić.

Poprawka: **nie ustawiać tego pola wcale**. `esp_lcd_new_dsi_bus()` sam dobiera źródło pod rewizję
układu, co działa i na starszych płytkach, i na naszej.

Diagnoza wyszła z dekodowania adresu awarii, nie z prób:
`_mipi_dsi_ll_set_phy_pllref_clock_source` w `hal/esp32p4/include/hal/mipi_dsi_ll.h`.

## Pułapki firmware'u interfejsu

Potwierdzone 2026-10-30 na właściwym firmware z tego repozytorium.

**Pula pamięci LVGL.** Wbudowany alokator LVGL dostaje domyślnie `LV_MEM_SIZE_KILOBYTES = 64`.
Pięć ekranów Pulpitu budowanych naraz to kilkaset obiektów i ta pula ich nie mieści. Po jej
wyczerpaniu `lv_malloc` **nie zgłasza błędu — zapętla się**, szukając wolnego bloku. Objaw jest
mylący: zadanie `main` zjada rdzeń 0, watchdog bije co 5 s na `IDLE0`, a adres awarii wypada
w ROM-owym `memcpy`, więc wygląda to na błąd układu, a nie na brak pamięci. Miejsce zawieszenia
przesuwa się przy każdej zmianie kodu, bo pula kończy się tam, gdzie akurat trafi.

Poprawka: oddać alokację stercie ESP-IDF, która sięga PSRAM.

```
CONFIG_LV_USE_CLIB_MALLOC=y
CONFIG_LV_USE_CLIB_STRING=y
CONFIG_LV_USE_CLIB_SPRINTF=y
CONFIG_SPIRAM_USE_MALLOC=y
```

Po tej zmianie, z pięcioma zbudowanymi ekranami, zostaje **442 kB pamięci wewnętrznej
i 31 938 kB PSRAM**. Aplikacja waży 1,13 MB przy partycji 7 MB.

**Wiązanie łatanego sterownika ST7701.** Manifest BSP producenta wskazywał ścieżkę
`../espressif__esp_lcd_st7701`, której w tym repozytorium nie ma — przy takim zapisie łatana kopia
w ogóle nie wchodziła do budowania. Sterownik jest teraz podpięty przez `PRIV_REQUIRES
esp_lcd_st7701_patched` w `components/board_bsp/CMakeLists.txt`. Dodatkowo `cu_pkg_define_version`
buduje nazwy makr z nazwy katalogu, więc w `components/esp_lcd_st7701_patched/CMakeLists.txt`
podstawiamy `ESP_LCD_ST7701_VER_*` pod `ESP_LCD_ST7701_PATCHED_VER_*`. Źródła producenta zostają
bez zmian.

**Wskaźnik ramki.** `CONFIG_ESP_SYSTEM_USE_FRAME_POINTER=y` rozdyma bootloader do 0x6650 przy
limicie 0x6000 i budowanie pada. Do diagnozy zawieszeń trzeba użyć logów etapowych albo przesunąć
tablicę partycji.

## Komunikaty, które wyglądają na błędy, a nie są

- `Install EK79007 LCD control panel` — **mylący**. Kilka linii niżej ten sam blok woła
  `esp_lcd_new_panel_st7701()`. Producent przerobił zawartość gałęzi, ale zostawił stary napis.
- `swap_xy is not supported by this panel` — BSP próbuje obrócić obraz sprzętowo, czego panel DPI
  nie potrafi. Przy natywnym pionie 480 × 800 to bez znaczenia.
- `GPIO 23 is not usable, maybe conflict with others` — podwójna rezerwacja pinu przy LEDC.
  Jasność i tak ustawia się na 100% bez błędu, a ekran świeci.
- `Detected boya flash chip but using generic driver` — działa; można później włączyć
  `SPI_FLASH_SUPPORT_BOYA_CHIP`.

## Jeszcze niepotwierdzone

- **Reset dotyku.** BSP podaje „niepodłączony", część źródeł społecznościowych wskazuje GPIO 3.
  Dotyk działa bez niego, więc sprawa jest otwarta i nieblokująca.
- **Pobór prądu i tryby oszczędzania** — nietestowane.
- **Obrót ekranu i kalibracja osi dotyku** przy innych orientacjach.

## ESP32-C6 i Wi-Fi

Potwierdzone 2026-10-30. ESP32-P4 nie ma własnego radia: Wi-Fi dowozi ESP32-C6 po SDIO, przez
`espressif/esp_wifi_remote` (API `esp_wifi_*`) i `espressif/esp_hosted` (transport).

| Parametr | Wartość | Skąd wiadomo |
| --- | --- | --- |
| Magistrala | SDIO, 4 bity, 40 MHz, slot 1 | log `eh_sdio` |
| Piny SDIO CLK / CMD / D0–D3 | 18 / 19 / 14, 15, 16, 17 | log `eh_sdio` |
| Reset koprocesora | GPIO **54** | log `eh_sdio` |
| Koprocesor | ESP32-C6, chip id `0x0d` | `eh_init_evt` |
| Firmware w C6 | **2.3.2**, fabryczne, nieruszane | `eh_init_evt` |
| Host | `esp_hosted` **3.0.10**, `esp_wifi_remote` **1.6.5** | `eh_init_evt` |
| Tryb | WLAN over SDIO, RPC V2, streaming | `eh_init_evt` |

**Nie trzeba niczego wgrywać do C6.** Fabryczny koprocesor 2.3.2 dogaduje się z hostem 3.0.10:
negocjacja kończy się na `Default RPC to V2` i `compatible streaming mode enabled`. Dwa ostrzeżenia
w logu są nieszkodliwe i wynikają wprost z różnicy wersji:

- `CP without SDIO SW_AGGR; compatible streaming mode enabled` — koprocesor nie umie agregacji,
  host schodzi do trybu zgodnego;
- `Missing SDIO_MODE TLV; Continue anyway.` — starsze firmware nie zgłasza tego pola.

Konfiguracji pinów **nie podajemy ręcznie**: `CONFIG_ESP_HOSTED_P4_DEV_BOARD_FUNC_BOARD=y` dobiera
je samo i są zgodne z tą płytką. Tak samo robi przykład producenta, który zresztą nie ustawia
w tej sprawie niczego.

Łącze wstaje ok. 1,2 s po starcie ekranu i nie opóźnia interfejsu — `esp_hosted` inicjalizuje się
przed `app_main`, ale resztę załatwia we własnym zadaniu. Potwierdzone na pełnej ścieżce: skan
z ekranu, wybór sieci, hasło z klawiatury, zapis do NVS i **samoczynne połączenie po restarcie
w 7,4 s od startu**, przy ekranie gotowym w 2,2 s.

### Dwie pułapki API Wi-Fi przez koprocesor

**Skan blokujący nie działa.** `esp_wifi_scan_start(cfg, true)` przez RPC wraca po ok. 100 ms
z zerem wyników. Trzeba zlecić skan nieblokująco i poczekać na `WIFI_EVENT_SCAN_DONE`.

**Koprocesor ma własne NVS.** Po `esp_wifi_start()` C6 sam próbuje łączyć się z siecią zapamiętaną
przez fabryczne demo i odrzuca wtedy skan z `ESP_ERR_WIFI_STATE`. Trzeba po starcie wywołać
`esp_wifi_disconnect()` i ponawiać skan. Tych danych nie kasujemy — pilot trzyma swoje w NVS
po stronie P4 i ustawia `WIFI_STORAGE_RAM`, więc nie nadpisuje cudzego wpisu.

**Skan potrafi zerwać połączenie.** Dotyczy to zarówno obejścia poprzedniej pułapki
(`esp_wifi_disconnect()` przed ponowieniem skanu wolno wywołać wyłącznie wtedy, gdy łącza
i tak nie ma), jak i samego skanowania przez koprocesor. Dlatego **nie skanujemy z automatu**
po wejściu na ekran Wi-Fi: przy działającym połączeniu lista pozostaje pusta, a skan uruchamia
dopiero dotknięcie przycisku. Po takim świadomym skanie powrót na sieć jest natychmiastowy,
bez odstępu przewidzianego na wyłączony router.

Dodatkowo faza usługi zmienia się na czas skanowania, więc stan połączenia trzeba trzymać
w osobnym polu: inaczej po każdym skanie interfejs melduje brak sieci, choć ta stoi.

Na marginesie: `esp_hosted` wypisuje nazwę sieci do logu na własnym poziomie logowania. Nasz kod
loguje wyłącznie fakt połączenia i siłę sygnału. Zrzutów z nazwą sieci nie wkleja się do
repozytorium.

## Kopia zapasowa i powrót

Pełny obraz 16 MB sprzed wgrania leży w `backups/guition-esp32p4-before-ui-2026-10-29.bin`,
SHA-256 zgodny z zapisem w `../operations/ai-handoff.md`. Powrót do stanu fabrycznego:

```bash
esptool --port /dev/cu.usbmodem101 write-flash 0x0 backups/guition-esp32p4-before-ui-2026-10-29.bin
```

Flash **nie był kasowany** — nadpisane zostały wyłącznie bootloader, tablica partycji i aplikacja.
Fabryczny podział różnił się od naszego: tablica pod 0x9000, aplikacja 7 MB pod 0x20000,
spiffs 6 MB pod 0x720000.
