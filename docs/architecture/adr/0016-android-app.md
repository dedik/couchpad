# ADR 0016 — Aplikacja na Androida w `phone/`

[English](0016-android-app.en.md)

- **Data:** 2026-11-08
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0001](0001-native-stack.md), [ADR 0002](0002-home-assistant-boundary.md), [ADR 0005](0005-fresh-state-after-wake.md), [ADR 0010](0010-start-screen-after-wake.md)
- **Zmienia:** zakres zasady „interfejs jest natywny: ESP-IDF + C++ + LVGL” z `AGENTS.md` — dotyczy panelu

## Problem

Ten sam pilot przydaje się poza panelem z ESP32-P4: na starym telefonie odłożonym na kanapę albo
jako drugi pilot w innym pokoju. Pierwsza próba uruchamiała kod LVGL firmware'u na telefonie przez
SDL2 i warstwę zgodności ESP-IDF/FreeRTOS. Działało, ale obraz 480 × 800 skalowany na ekran
telefonu był rozmyty, a dotyk i system (paski, ekran blokady, launcher) obsługiwało się z trudem.

Aplikacja powstała najpierw w osobnym repozytorium obok. Przez to dokumentacja i zrzuty
rozjeżdżały się z firmware'em, a publiczne repozytorium nie mówiło, że wersja na telefon istnieje.

## Decyzja

**Pilot ma drugą wersję: natywną aplikację na Androida (Kotlin + Jetpack Compose) w katalogu
`phone/` tego repozytorium.**

- `phone/native/` to aplikacja główna. `phone/android`, `phone/compat` i `phone/platform` to
  wcześniejszy port SDL/LVGL — zostaje jako referencja i symulator na Macu, nie jest rozwijany.
- Aplikacja jest **drugim klientem tego samego kontraktu**: `multimedia_remote/v1`, tematy Z906
  (ADR 0006) i Glasshouse (ADR 0008), ten sam pakiet Home Assistanta. Nie dodaje własnych tematów;
  zmiana kontraktu idzie przez `mqtt-contract.md` i dotyczy obu wersji.
- **Wzorcem jest panel.** Ekrany, ustawienia, teksty i zasady UX (`docs/ux/`, ADR-y) projektujemy
  dla panelu, a aplikacja je odwzorowuje 1:1, w pionie i w poziomie. Różnice wynikają tylko
  z platformy: Wi-Fi i jasność ekranu należą do systemu, aplikacja może zastąpić launcher
  i obracać się razem z telefonem.
- Zasady panelu obowiązują aplikację tam, gdzie mają sens: świeży stan po wybudzeniu (ADR 0005),
  ekran startowy po wybudzeniu (ADR 0010), dotyk nigdy nie czeka na sieć, brak sekretów
  w repozytorium (dane brokera wpisuje się na telefonie).
- Zasada „ESP-IDF + C++ + LVGL, bez innego frameworka GUI” dotyczy **panelu**. Kotlin i Compose
  są dozwolone wyłącznie w `phone/`.
- Firmware nie zależy od `phone/`. Port SDL korzysta z komponentów firmware'u (`..`), ale niczego
  w nich nie zmienia.

## Skutki

- Jedno repozytorium, jedna dokumentacja: główne README opisuje obie wersje, a zrzuty aplikacji
  leżą w `phone/docs/screenshots/` (skrypt `phone/tools/screenshots.sh`).
- Nowy ekran albo zmiana UI na panelu oznacza zadanie także dla aplikacji, żeby wersje się nie
  rozjechały. Stan prac aplikacji: `phone/docs/PROGRESS.md`.
- Budowanie aplikacji wymaga JDK 17 i Android SDK, ale nie jest potrzebne do budowania firmware'u.
