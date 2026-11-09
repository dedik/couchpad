# ADR 0018 — Grzejniki: głowice termostatyczne w pomieszczeniach

[English](0018-radiators.en.md)

- **Data:** 2026-11-09
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0009](0009-climate-via-home-assistant.md), [ADR 0014](0014-rooms-and-lights.md), [ADR 0017](0017-room-climate-card.md)

## Problem

W salonie, sypialni i łazience są głowice termostatyczne SONOFF TRVZB (Zigbee2MQTT, encje
`climate.*`), a w biurze dochodzi czwarta. Pilot ich nie widział: pokój pokazywał tylko
światło i klimatyzację. Kuchnia i przedpokój grzejnika nie mają i mieć nie będą.

Głowica mierzy temperaturę przy kaloryferze, więc jej `current_temperature` zawyża odczyt
(salon: głowica 24,5°, czujnik pokoju 22,6°). Ma też własny harmonogram tygodniowy
(`text.*_weekly_schedule_<dzień>`, sześć punktów `HH:MM/stopnie` na dzień), używany w trybie
`auto`.

## Decyzja

- **Karta „Grzejnik” w pokoju i ekran urządzenia** według standardu zakładki Dom: tarcza
  nastawy (reguła 3), zasilanie w nagłówku, kafle obok. Makiety: `docs/ux/mockup-dom/rooms.js`.
- **Temperatura w pokoju wyłącznie z czujnika pokoju** — tego samego co karta „W pokoju”
  (ADR 0017, `indoor.rooms` w wiadomości pogody). Snapshot grzejnika celowo nie ma pola z
  temperaturą głowicy. Brak odczytu czujnika chowa znacznik i podpis „w pokoju”, nie podstawia głowicy.
- **Tryby:** `heat` — „Grzanie” (stała nastawa), `auto` — „Harmonogram” (plan w głowicy).
  Wyłączenie to zasilanie w nagłówku (`climate.turn_off`), włączenie wraca do ostatniego trybu
  (`climate.turn_on`), jak przy klimie.
- **Harmonogram tylko do podglądu.** Bridge liczy następny punkt planu, który zmienia
  temperaturę (`next: {at, target}`), a pilot pokazuje go w trybie Harmonogram. Edycja zostaje
  w Home Assistancie / Zigbee2MQTT — sześć punktów na siedem dni to za dużo na pilota.
- **Tarcza:** zakres z encji przycięty do 5–30 °C (encja pozwala 4–35), krok z encji (0,5).
  Łuk w kolorze od niebieskiego (zimno) przez bursztyn do czerwieni (gorąco) — obraz pierścienia
  jako tekstura łuku. Biała kropka na łuku to temperatura z czujnika pokoju. Jedno polecenie na
  gest, jak przy klimie.
- **Kafle:** Tryb (Grzanie, Harmonogram), Teraz (zawór: grzeje / nie grzeje z `hvac_action`;
  okno; bateria głowicy; w Harmonogramie następny punkt, w Grzaniu różnica cel – pokój), Szybko
  (17°, 19°, 21°, 23° — ustawiają nastawę).
- **Okno z kontaktronu pokoju, nie z głowicy.** TRVZB nie podaje, że okno jest otwarte:
  `switch.*_open_window` to włącznik funkcji wykrywania, nie odczyt. Bridge bierze stan z czujnika
  otwarcia okna przypisanego do pokoju (`window` w `radiator_rooms`, `binary_sensor`). Pokój bez
  czujnika nie ma kafla „Okno”. Otwarte okno: karta mówi „okno otwarte” zamiast „grzeje”.
- **Kontrakt:** sekcja `radiators` w snapshocie i akcje `radiator.power.set`,
  `radiator.mode.set`, `radiator.target.set` z identyfikatorem grzejnika. Zmiana addytywna,
  wersja zostaje `v1`. Encje zna tylko bridge (tabela `radiator_rooms`).
- **Ikony pixel art:** `trv_auto` (kalendarz z zegarem), `mode_heat` (ten sam identyfikator HA
  co tryb klimy), zawór `trv_valve_on/off`, okno `trv_window_open/closed`, bateria `trv_battery_ok/low`, `trv_next`, `trv_delta`;
  studzienka karty `dev_radiator` (grzeje) i `dev_radiator_idle`.

## Skutki

- Snapshot rośnie o ok. 220 bajtów na grzejnik. Firmware czyta najwyżej cztery.
- Nowa głowica to wpis w `radiator_rooms` i encje w wyzwalaczach automatyzacji
  „snapshot po zmianie grzejnika”, bez firmware'u (o ile pokój jest jednym z sześciu).
- Głowice mają dziś `temperature_sensor_select = internal`, więc temperatura z czujnika pokoju,
  którą automatyzacje „czujnik temperatury do termostatu” wpisują do
  `number.*_external_temperature_input`, nie steruje zaworem. To konfiguracja głowic, poza tą
  decyzją — pilot i tak pokazuje czujnik pokoju.
