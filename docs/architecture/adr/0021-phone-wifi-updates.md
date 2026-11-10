# ADR 0021 — Aktualizacja aplikacji na Androida przez Wi-Fi

[English](0021-phone-wifi-updates.en.md)

- **Data:** 2026-11-10
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0016](0016-android-app.md)

## Problem

Telefony z pilotem stoją w domu jako stałe piloty. Każda nowa wersja wymagała podłączenia
telefonu kablem do laptopa i `adb install`. Debugowanie bezprzewodowe adb nie rozwiązuje tego
na stałe: Xiaomi wyłącza je po restarcie i zmianie sieci, a i tak potrzebny jest laptop.

## Decyzja

- **Aplikacja sama proponuje aktualizację.** Laptop publikuje nową wersję, a każdy telefon
  z pilotem dowiaduje się o niej z brokera, z którym i tak jest połączony.
- **Manifest** to retained wiadomość na `multimedia_remote/app/android` (QoS 1), wspólna dla
  wszystkich telefonów:
  ```json
  {"version_code": 57, "version_name": "0.3.57",
   "url": "http://<ha>:8123/local/pilot/pilot-57.apk",
   "sha256": "<64 znaki hex>", "size": 14680064}
  ```
  Aplikacja subskrybuje temat po każdym połączeniu z brokerem (`PilotRepository.release`)
  i pokazuje wersję tylko wtedy, gdy `version_code` jest wyższy od zainstalowanego.
  Niepełny manifest jest ignorowany.
- **APK leży w Home Assistancie**, w katalogu `www/pilot` konfiguracji (adres `/local/pilot/`),
  bez logowania, tylko w sieci domowej. Nazwa pliku zawiera numer wersji, więc żaden cache nie
  poda starego pliku; skrypt zostawia dwie ostatnie wersje.
- **Instalacja** (`data/AppUpdater.kt`): pobranie do `cacheDir`, sprawdzenie sha256, sesja
  `PackageInstaller`. Na Androidzie 12+ sesja prosi o `USER_ACTION_NOT_REQUIRED`
  (uprawnienie `UPDATE_PACKAGES_WITHOUT_USER_ACTION`): system instaluje bez okna, jeśli pilot
  jest instalatorem z rekordu samego siebie. Tak jest po pierwszej aktualizacji przez Wi-Fi;
  instalacja kablem zwykłym `adb install` to zeruje (wtedy `adb shell pm install -r -i
  app.couchpad.pilot …`). Starsze systemy i pierwsza aktualizacja pokazują okno Androida.
  Przy pierwszej aktualizacji Android 8+
  wymaga jednorazowej zgody „Instaluj nieznane aplikacje” dla pilota; strona Telefon prowadzi
  do tego ekranu i po powrocie sama sprawdza zgodę.
- **Interfejs:** *Ustawienia → Telefon → Wersja aplikacji*. Bez nowszej wersji: „aktualna”.
  Z nowszą: „Zainstaluj wersję …”, potem postęp pobierania, „Instalowanie…” albo błąd
  z „Spróbuj ponownie”.
- **Numer wersji** to liczba commitów w repozytorium (`git rev-list --count HEAD` w
  `app/build.gradle`), `versionName` = `0.3.<numer>`. Rośnie z każdym commitem bez ręcznego
  podbijania.
- **Publikacja:** `phone/tools/publish-update.sh` buduje APK, wgrywa go przez SSH do
  `www/pilot`, sprawdza, że jest dostępny pod adresem, i publikuje manifest przez
  `mqtt.publish` w HA. Odmawia publikacji z niezacommitowanymi zmianami (`--dirty` wymusza),
  żeby numer wersji zawsze wskazywał commit. Adresy, konto SSH i polecenie podające token HA
  są w `phone/tools/update.env` poza gitem (wzór: `update.env.example`).

## Skutki

- Wszystkie wersje muszą być podpisane tym samym kluczem. Dziś to klucz debug z laptopa
  (`~/.android/debug.keystore`); jego utrata oznacza jednorazową reinstalację kablem na każdym
  telefonie (z utratą ustawień pilota).
- Pierwsza aktualizacja wymaga zgody i potwierdzeń: na Samsungu (One UI) jednego, na Xiaomi
  (HyperOS/MIUI) kilku ekranów MIUI. Kolejne na Androidzie 12+ to jedno dotknięcie
  „Zainstaluj wersję …” w pilocie. Sprawdzone 2026-11-10 na obu.
- Okno Androida da się pominąć, ale nie skanery systemu: każda wersja ma nową sumę, więc
  Google Play Protect („App scan recommended”) albo skaner producenta (MIUI) mogą zatrzymać
  aktualizację i poprosić o decyzję. Aplikacja tych ustawień nie zmienia.
- Pilot pokazuje się nad ekranem blokady (ADR 0010), a okna zgody i instalatora Androida
  już nie — telefon trzeba odblokować.
- Po instalacji Android zabija proces pilota; jako ekran główny wraca sam po Home.
- Plik APK w `/local` jest dostępny dla każdego w sieci domowej bez logowania. Nie zawiera
  sekretów — ustawienia brokera trzyma telefon, nie APK.
