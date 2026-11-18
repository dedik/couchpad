# ADR 0022 — Ekran startowy: kafelki z samą ikoną, zasilanie i pigułka stanu

[English](0022-start-tiles-power.en.md)

- **Data:** 2026-11-18
- **Status:** przyjęta
- **Zmienia:** [ADR 0010](0010-start-screen-after-wake.md) (start „niczego nie steruje”, zdanie
  stanu na kafelku), [ADR 0020](0020-remote-tab.md) (nazwa i stan u góry górnego rzędu)

## Problem

Najczęstsza rzecz po wzięciu pilota do ręki to włączenie albo wyłączenie urządzenia. Dziś trzeba
wejść na ekran urządzenia i dopiero tam nacisnąć zasilanie. Kafelki startu powtarzały przy tym
nazwę i jedno zdanie stanu, które ikona i ekran urządzenia mówią i tak.

## Decyzja

- **Kafelki bez podpisów.** Każdy kafelek to duża ikona na środku: w pionie 144 px (rysunek
  48 px w 3 ×, `icon_*_xl`), w poziomie 96 px (2 ×). Zdanie stanu i nazwa znikają z ekranu;
  w aplikacji nazwa zostaje jako opis dla TalkBacka.
- **Zasilanie na kafelku.** TV, Apple TV i Głośniki mają w lewym górnym rogu ten sam przycisk
  zasilania co nagłówek ekranu urządzenia (48 px, 64 px pod palcem; w aplikacji 40 dp, styl
  `Solid`). Wysyła ten sam zamiar (`SetPower` z `TvPower` / `AtvPower` / `ZPower`), stan
  oczekujący pokazuje obwódka, a potwierdza go snapshot. Przed świeżym stanem przycisk jest
  przygaszony i nieaktywny (ADR 0005). Dom nie ma przycisku. Dotknięcie reszty kafelka otwiera
  ekran urządzenia jak dotąd.
- **TV:** włączenie z czuwania wymaga Wake-on-LAN, który na telewizorze jest wyłączony
  (ADR 0008), więc przycisk na kafelku — jak w nagłówku — pokazuje stan, ale niczego nie zmienia.
- **Pigułka stanu nad zegarem:** siła Wi-Fi (3 łuki, RSSI ≥ −60 / ≥ −70 dBm) i zasilanie.
  Panel: RSSI odświeżane co 10 s w zadaniu Wi-Fi wyłącznie przy widocznym starcie; ogniwa
  jeszcze nie ma, więc „USB” (rysowanie baterii czeka na pomiar w `board`). Aplikacja: bateria
  telefonu i RSSI z lokalnych API Androida. Bez nowych tematów MQTT.
- Projekt: `docs/ux/mockup-tile-power`, `docs/ux/mockup-status` (wariant B).

## Skutki

- Ekran startowy po raz pierwszy steruje urządzeniami — z tymi samymi zasadami co ekrany
  urządzeń (stan oczekujący, brak sterowania przed świeżym snapshotem).
- Na kafelku nie widać już „brak połączenia” dla pojedynczego urządzenia; zostaje stopka
  „Połączono z domem” i przygaszone przyciski.
- Wyłączenie to jedno dotknięcie, bez potwierdzenia — łatwo trafić przypadkiem. Do obserwacji.
- Bez podpisów nic nie ginie pod kołem Pilota, więc górny rząd nie potrzebuje już tekstu u góry.
