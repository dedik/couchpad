# ADR 0009 — Klimatyzacja przez Home Assistanta i kontrakt v1

[English](0009-climate-via-home-assistant.en.md)

- **Data:** 2026-11-03
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0002](0002-home-assistant-boundary.md), [ADR 0007](0007-apple-tv-via-home-assistant.md)

## Problem

Ekran `DOM` ma kafelek klimatyzacji i jej podstronę (makieta: `docs/ux/mockup-pulpit`,
kod: `ui/screens/screen_climate.cpp`), ale na żywo nic za nimi nie stało — działały
wyłącznie na danych udawanych. Klimatyzacja nie ma własnego kontrolera MQTT. W Home
Assistancie jest encją `climate` (integracja Midea), która ma już wszystko, czego ekran
potrzebuje: tryby, temperaturę docelową i w pokoju, zakres, krok, biegi nawiewu i żaluzje.

## Decyzja

Klimatyzacja idzie tą samą drogą co Apple TV: kontraktem
[`multimedia_remote/v1`](../mqtt-contract.md), a tłumaczeniem na encję zajmuje się bridge
w [`../../../homeassistant/`](../../../homeassistant/).

```text
pilot → broker → Home Assistant (climate.*) → klimatyzator
```

Kontrakt dostaje sekcję `climate` w snapshocie i akcje `climate.*`. Zmiana jest addytywna:
stare klienty ignorują nieznaną sekcję, a bridge — nieznane akcje. Wersja w temacie zostaje `v1`.

Ustalenia:

- **Identyfikatory, nie nazwy.** Tryby i opcje płyną jako identyfikatory HA (`cool`,
  `medlow`, `Full`). Polskie nazwy dobiera ekran; nieznany identyfikator pokazuje tak, jak
  przyszedł. Listy (`hvac_modes`, `fan_modes`, `swing_modes`, `swing_horizontal_modes`)
  przychodzą z encji, firmware ich nie zaszywa.
- **Zakres i krok z encji** (`min_temp`, `max_temp`, `target_temp_step`). Temperatury w JSON-ie
  są liczbami w stopniach; firmware trzyma je w dziesiątych.
- **Włączenie wraca do ostatniego trybu** (`climate.turn_on`), wybór trybu przy wyłączonej
  klimie zarazem ją włącza (`climate.set_hvac_mode`). Wyłączenie to `climate.turn_off`.
- **Temperatura to jedno polecenie na gest.** Pilot wysyła wartość końcową: po puszczeniu
  palca na tarczy albo 0,7 s po ostatnim stuknięciu w −/+.
- **Snapshot po zmianie stanu i wybranych atrybutów** (temperatura docelowa i w pokoju,
  nawiew, żaluzje) — nie po każdym atrybucie integracji, bo Midea zmienia ich wiele naraz.

## Skutki

- Stan klimy przychodzi tym samym klientem HA co Apple TV. Brak połączenia z brokerem gasi
  oba; brak sekcji `climate` w snapshocie (bridge sprzed tej decyzji) gasi samą klimę.
- Snapshot pilota urósł o ok. 1,1 kB. Zdarzenia z klientów transportu niosą wyłącznie stan
  swojego urządzenia i powstają poza stosem zadania MQTT (por. handoff, 2026-11-03).
- Encja jest na sztywno w bridge'u (`climate.living_room_ac`). Druga klima to druga sekcja i ADR.
