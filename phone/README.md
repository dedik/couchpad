# Pilot na telefonie

[English](README.en.md)

> Część repozytorium [Couchpad](../README.pl.md): druga wersja pilota obok firmware'u panelu
> z ESP32-P4 ([ADR 0016](../docs/architecture/adr/0016-android-app.md)).

Pilot Couchpad (TV, Apple TV, Logitech Z906, światła i klimatyzacja przez Home Assistanta)
jako aplikacja na Androida. Prototyp: Galaxy M23 5G, docelowo Galaxy S5 (Android 6).

Aplikacja główna to **natywna aplikacja Kotlin + Jetpack Compose w [`native/`](native/)**.
Odwzorowuje 1:1 interfejs i ustawienia firmware'u płytki (katalog główny tego repozytorium,
[`../`](../README.pl.md)) i mówi tym samym kontraktem MQTT. Firmware od aplikacji nie zależy.

![Ekrany w pionie](docs/screenshots/overview-portrait.png)

![Ekrany w poziomie](docs/screenshots/overview-landscape.png)

![Ekrany w pionie — styl jasny](docs/screenshots/overview-portrait-light.png)

![Ekrany w poziomie — styl jasny](docs/screenshots/overview-landscape-light.png)

Wszystkie ekrany: [`docs/screenshots/portrait/`](docs/screenshots/portrait/) i
[`docs/screenshots/landscape/`](docs/screenshots/landscape/) — Start, TV, Apple TV, Z906, Dom,
pokój, lampa, efekty, żarówka, klimatyzacja i ustawienia; w stylu jasnym w podkatalogach `light/`.

## Co robi

- **Łączność:** jedno połączenie MQTT. Z906 wprost na tematach kontrolera (ADR 0006),
  telewizor wprost przez Glasshouse (ADR 0008), Apple TV, klima, światła, pogoda i kanały
  kontraktem `multimedia_remote/v1` przez bridge HA. Po każdym otwarciu łączy się od nowa
  i pokazuje sterowanie dopiero po świeżym stanie (ADR 0005); w tle się rozłącza.
- **Ekrany jak na płytce,** w pionie (zakładki na dole) i w poziomie (boczny pasek, szuflady
  w zakładce Dom). W poziomie pasek wchodzi w pas obok przedniej kamery.
- **Launcher:** pilot może być ekranem głównym telefonu — startuje razem z nim, Home wraca
  na ekran startowy, a po zapaleniu ekranu pilot pokazuje się od razu, nad ekranem blokady
  (bez PIN-u nie trzeba go przesuwać), zawsze na ekranie startowym (ADR 0010). Ustawia się
  go w *Ustawienia → Telefon → Ustaw jako ekran główny*; tam też lista aplikacji telefonu
  i wejście do ustawień Androida.
- **Styl OLED** (tylko w aplikacji): *Ustawienia → Telefon → Styl ciemny na tym telefonie*.
  Karty i kontrolki w środku czarne, sam obrys, przygaszony tekst i miękki akcent na dużych
  polach — dla ekranów OLED/AMOLED; na LCD zostaw standard. Pilot narzuca jasność z
  *Ustawienia → Ekran → Jasność*, więc na OLED-zie ustaw ją niżej (40–60%)
  ([ADR 0019](../docs/architecture/adr/0019-phone-oled-style.md)).
- **Aktualizacja przez Wi-Fi:** po `phone/tools/publish-update.sh` na laptopie każdy telefon
  pokaże w *Ustawienia → Telefon* „Zainstaluj wersję …”; pobiera APK z Home Assistanta,
  sprawdza sumę i instaluje. Na Androidzie 12+ bez okna potwierdzenia (Samsung), na Xiaomi
  (HyperOS) z jednym potwierdzeniem. Za pierwszym razem trzeba zezwolić pilotowi instalować
  aplikacje; Play Protect albo skaner producenta mogą przy nowej wersji poprosić o decyzję.
  Pilot wraca sam po aktualizacji tylko jako ekran główny
  ([ADR 0021](../docs/architecture/adr/0021-phone-wifi-updates.md)).
- **Ustawienia 1:1:** MQTT (puste identyfikatory wykrywa sam), ekran (orientacja, styl,
  jasność, tryb nocny, wygaszanie), dźwięk, pokój i czas, klawiatura i gładzik, diagnostyka.

## Budowa i instalacja

Wymaga JDK 17 (`brew install openjdk@17`) i Android SDK z `platforms;android-35`,
`build-tools;34.0.0`. Numer wersji to liczba commitów (`versionCode`, `0.3.<n>`).

```bash
cd native
export JAVA_HOME=$(brew --prefix openjdk@17)/libexec/openjdk.jdk/Contents/Home
echo "sdk.dir=$HOME/Library/Android/sdk" > local.properties
./gradlew assembleDebug
adb push app/build/outputs/apk/debug/app-debug.apk /data/local/tmp/pilot.apk
adb shell pm install -r -i app.couchpad.pilot /data/local/tmp/pilot.apk
adb shell am start -n app.couchpad.pilot/.MainActivity
./gradlew testDebugUnitTest   # testy logiki bez telefonu
```

`-i app.couchpad.pilot` zostawia pilota instalatorem samego siebie — zwykłe `adb install`
to zeruje i następna aktualizacja przez Wi-Fi znów pokaże okno potwierdzenia.

Wydanie dla telefonów w domu przez Wi-Fi (po commicie; konfiguracja w `tools/update.env`
poza gitem, wzór `tools/update.env.example`):

```bash
tools/publish-update.sh
```

Dane brokera wpisuje się na telefonie (Ustawienia → MQTT). Nie trafiają do repozytorium.

Zrzuty ekranu z podłączonego telefonu: `tools/screenshots.sh portrait|landscape [light]`
(z `light` w stylu jasnym, do `docs/screenshots/<orientacja>/light/`).

## Port LVGL (referencja)

Pierwsza wersja uruchamiała kod LVGL firmware'u przez SDL2: `compat/` (ESP-IDF i FreeRTOS
na wątkach systemu, NVS w pliku, klient MQTT z API esp-mqtt), `platform/` i `android/`.
Działa także jako symulator na Macu (`cmake -S . -B build/sim && cmake --build build/sim`),
ale obraz 480 × 800 skalowany na telefonie był rozmyty, więc rozwijamy aplikację natywną.

Stan prac i kolejne kroki: [`docs/PROGRESS.md`](docs/PROGRESS.md).
