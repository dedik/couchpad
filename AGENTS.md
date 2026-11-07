# Instrukcja dla kolejnych AI i osób rozwijających projekt

[English](AGENTS.en.md)

## Zanim zaczniesz

1. Przeczytaj `docs/architecture/README.md`, `docs/architecture/mqtt-contract.md`, `docs/ux/tv-and-z906-ui.md` oraz `docs/operations/ai-handoff.md`. Przy pracy nad zakładką Dom — także `docs/ux/home-screen.md` (standard: hierarchia, reguły, komponenty, dodawanie urządzeń).
2. Sprawdź `git status --short --branch` i nie usuwaj cudzych, niezatwierdzonych zmian.
3. Przed modyfikacją kodu sprzętowego przeczytaj `docs/hardware/bring-up.md`.

## Niezmienne założenia v1

- Docelowa płytka to Guition `JC4880P443C_I_W_Y`: ESP32-P4 obsługuje aplikację, ekran i dotyk; ESP32-C6 zapewnia łączność bezprzewodową.
- Panel nie steruje bezpośrednio wzmacniaczem Logitech Z906, nie używa IR i nie implementuje jego protokołu UART. Jest klientem Home Assistant przez MQTT.
- Interfejs jest natywny: ESP-IDF + C++ + LVGL. Nie dodawaj serwera WWW, HTML, JavaScriptu ani alternatywnego frameworka GUI bez ADR.
- Domyślny tryb bezczynności oszczędza energię. Po dotyku pilot może poświęcić do około 3 s na wybudzenie, połączenie z MQTT i wymuszoną synchronizację; ekran sterowania pokazuje wyłącznie nowy, potwierdzony snapshot TV i Z906. Nie pokazuj zapamiętanego stanu jako aktualnego.
- Nie zapisuj haseł, tokenów, nazw prywatnej sieci ani adresów domowej infrastruktury w repozytorium, logach i przykładach.
- Nie wgrywaj firmware'u, nie kasuj flasha i nie zmieniaj fizycznego okablowania bez wyraźnej prośby użytkownika.

## Zasady jakości

- Wszystkie wywołania LVGL wykonuje wyłącznie zadanie UI.
- Dotknięcie nie może czekać na sieć; po akcji UI pokazuje stan oczekujący, a stan potwierdza wiadomość MQTT.
- Gdy zmieniasz granicę modułu, format MQTT, model stanu albo zależność, dodaj ADR i zaktualizuj dokumentację architektury.
- Kod sprzętowy powstaje dopiero po potwierdzeniu go na tym konkretnym egzemplarzu płytki. Nie kopiuj pinów z podobnych modeli Guition.
- Zatwierdzony ekran Z906 to układ Soundstage opisany w `docs/ux/tv-and-z906-ui.md`; nie zastępuj go uproszczoną kartą. Wspólne zakładki ekranów są obowiązkowe na ekranach codziennego sterowania: na dole w pionie, w bocznym pasku w poziomie ([ADR 0013](docs/architecture/adr/0013-landscape-orientation.md)). Nowy ekran projektuj w obu orientacjach.
