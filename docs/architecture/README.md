# Architektura firmware'u

[English](README.en.md)

## Cel

Firmware ma być szybkim, dotykowym pilotem do TV, Logitech Z906 oraz wybranych akcji Home Assistant. Płytka nie jest kolejnym kontrolerem urządzeń: jest responsywnym klientem interfejsu, a Home Assistant pozostaje centralnym miejscem automatyzacji i integracji.

Projekt bazuje na Guition `JC4880P443C_I_W_Y`: ESP32-P4, ESP32-C6, ekran IPS 4,3 cala 480 × 800 ze sterownikiem ST7701S oraz dotyk pojemnościowy. Oficjalne materiały producenta są punktem odniesienia dla przykładu rozruchowego i schematu: <https://github.com/guitionofficial/P4-series/tree/master/JC4880P443C_I_W/JC4880P443C_I_W>.

## Decyzje obowiązujące

- **Framework:** ESP-IDF, C++17 i LVGL 9.
- **Łączność aplikacyjna:** MQTT z brokerem używanym przez Home Assistant.
- **Granica integracji:** panel publikuje wyłącznie opisane polecenia pilota; Home Assistant wykonuje usługi TV i Z906 oraz publikuje stan agregowany.
- **Ekran:** portret 480 × 800, RGB565, prosty natywny interfejs bez warstwy webowej.
- **Mechanika:** nie należy do tego repozytorium; jest w osobnym repozytorium `../enclosure`.
- **Wybudzanie:** pierwszy dotyk uruchamia połączenie z MQTT i wymuszoną synchronizację z Home Assistantem. Interaktywny ekran TV/Z906 pojawia się z nowym, potwierdzonym snapshotem, docelowo w ciągu około 2–3 s.

Wersję ESP-IDF przypinamy po udanym teście ekranu, dotyku i ESP32-C6 na tej płytce. Nie aktualizujemy jej podczas implementacji funkcji UI bez osobnego testu sprzętowego.

## Mapa systemu

```text
dotyk
  │
  ▼
LVGL UI ── UiIntent ──► kontroler aplikacji ──► MQTT ──► Home Assistant
  ▲                           │                                │
  │                           ▼                                ▼
  └──── render(AppState) ◄─ magazyn stanu ◄─ snapshot stanu ◄─ integracje TV/Z906

ESP32-P4: aplikacja, MIPI-DSI, LVGL, touch, MQTT
ESP32-C6: Wi-Fi/BLE jako współprocesor łączności
```

## Wybudzenie i aktualny stan urządzeń

W normalnym okresie bezczynności pilot przechodzi w tryb oszczędzania energii. Po dotyku może wykorzystać około 2–3 s na wybudzenie, uruchomienie Wi-Fi przez ESP32-C6 oraz MQTT. Ten czas jest przeznaczony na uzyskanie **nowego** stanu z Home Assistanta, a nie tylko odtworzenie danych z RAM.

Sekwencja wybudzenia:

1. Dotyk wybudza urządzenie i pokazuje lekki ekran „Łączenie z domem”.
2. Pilot łączy się z Wi-Fi oraz brokerem MQTT i publikuje `remote.sync.request`.
3. Home Assistant pobiera/scala bieżące stany integracji TV i Z906 oraz publikuje pełny snapshot o nowej rewizji.
4. Dopiero po jego odebraniu pojawia się interaktywny ekran Home, TV lub Z906.

Jeżeli połączenie lub odpowiedź nie zmieści się w docelowym czasie, pilot nie pokazuje starych danych jako aktualnych. Zamiast tego pozostaje na ekranie łączenia z możliwością ponowienia. Konkretna technika niskiego poboru (light sleep czy deep sleep) zostanie wybrana po teście, czy dotyk tej płytki może pewnie wybudzać wybrany tryb.

## Struktura kodu

```text
main/                       composition root; uruchamia moduły
components/
  board/                    zweryfikowany tylko na tej płytce start ekranu, dotyku i C6
  core/                     kolejki zdarzeń, zegar, identyfikatory, podstawowe typy
  domain/                   AppState, UiIntent, Command, DeviceSnapshot
  application/              kontrolery i reguły: intent → command / state → model UI
  transport/                Wi-Fi przez ESP32-C6, docelowo MQTT i kodowanie kontraktu v1
  storage/                  ustawienia urządzenia w NVS, bez sekretów w źródłach
  ui/                       motyw, komponenty, ekrany, wiązanie danych LVGL
  diagnostics/              metryki, kontrolowany log i ekran diagnostyczny
docs/                       kontrakty, ADR-y, procedury i historia decyzji
test/                       testy jednostkowe logiki niezależnej od sprzętu
```

Zależności przebiegają wyłącznie w dół: `ui → application → domain`, a `transport` i `storage` są adapterami wywoływanymi przez `application`. UI nie importuje klienta MQTT ani Wi-Fi, a moduły domenowe nie znają LVGL ani ESP-IDF.

Adaptery pracują we własnych zadaniach i oddają wyniki kolejkami, a `application` opróżnia je z zadania UI. Dzięki temu dotknięcie nigdy nie czeka na sieć, a wszystkie wywołania LVGL zostają w jednym zadaniu (ADR 0003). Ten sam wzorzec obsługuje dziś symulowane źródło snapshotów i usługę Wi-Fi.

Prawdziwe źródło snapshotów (`LiveSource`) składa trzy części: Z906 wprost z kontrolera (ADR 0006), Apple TV kontraktem v1 przez Home Assistanta (ADR 0007) i telewizor LG wprost z serwera Glasshouse na TV (ADR 0008). Na zewnątrz to jedno źródło z jedną rewizją, więc `AppController` i UI nie wiedzą o podziale.

## Kolejność implementacji

1. Zweryfikować sprzęt i zapisać rzeczywiste parametry w `docs/hardware/`.
2. Stworzyć minimalny `AppState`, kolejkę zdarzeń i statyczny ekran główny LVGL.
3. Dodać ekran TV i Z906 na danych symulowanych.
4. Wdrożyć kontrakt MQTT i połączyć go z Home Assistant.
5. Dodać konfigurację Wi-Fi/MQTT, diagnostykę oraz aktualizację OTA.

Nie łączymy etapu pierwszego z baterią, CAD-em, kamerą, air mouse ani bezpośrednim protokołem webOS.
