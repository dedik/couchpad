# ADR 0017 — Temperatura i wilgotność na podstronie pomieszczenia

[English](0017-room-climate-card.en.md)

- **Data:** 2026-11-09
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0014](0014-rooms-and-lights.md), [ADR 0015](0015-indoor-average-on-start-screen.md)

## Problem

Podstrona pomieszczenia pokazywała tylko urządzenia. Odczyt czujnika pokoju był widoczny
wyłącznie jako średnia całego domu na ekranie startowym (ADR 0015), a w pomieszczeniach są
już czujniki temperatury i wilgotności, a w trzech także głowice termostatyczne.

## Decyzja

- **Karta „W pokoju” nad urządzeniami.** Dwa odczyty obok siebie: ikona pixel art (termometr,
  kropla, `tools/draw_device_icons.py`, siatka 24 w 2 ×), duża liczba i pod nią ocena w kolorze:
  temperatura 19–24,5 °C „komfortowo”, niżej „chłodno”, wyżej „ciepło”; wilgotność 40–60 %
  „komfortowo”, 35–39 % „lekko sucho”, niżej „sucho”, powyżej 60 % „wilgotno”. Bez wykresu.
  W pionie karta stoi pod paskiem pokoi z podpisem `W POKOJU`; w poziomie jest pierwszą,
  węższą kolumną rzędu kart (temperatura nad wilgotnością).
  *Zmiana 2026-11-09:* ocenę słowem i kropkę usunięto na prośbę użytkownika — nic nie mówiła;
  zostały ikona i liczba.
- **Źródło: czujnik pokoju, nie głowica.** Głowice „Termostat” nie podają wilgotności, a ich
  temperatura jest mierzona przy kaloryferze — to wbrew ADR 0015 zawyżałoby odczyt. Bridge
  bierze czujniki „<Pokój> - Czujnik temperatury”, te same, które automatyzacje przekazują
  głowicom jako temperaturę zewnętrzną. Kuchnia jest otwarta na salon, a przedpokój na biuro,
  więc biorą ich czujniki (jak w średniej z ADR 0015). Pokój bez czujnika nie ma karty.
- **Jedzie z pogodą.** Pole `indoor.rooms` w wiadomości `weather`: lista `{room, temperature,
  humidity}`. To treść, nie stan urządzenia: retained, pokazywana z pamięci, chowana po
  6 godzinach jak pogoda (ADR 0011). Wiadomość i tak wychodzi po każdej zmianie tych czujników.
  Zmiana addytywna — starszy pilot pole ignoruje.
- **Ikona klimatyzacji:** karta klimy w salonie dostaje ikonę klimatyzatora (`dev_aircon`)
  zamiast ikony trybu chłodzenia.

## Skutki

- Bridge zna listę czujników per pokój w skrypcie `multimedia_remote_publish_weather`
  (`room_sensors`). Nowy czujnik to zmiana bridge'a, nie firmware'u.
- Wiadomość pogody rośnie o około 250 bajtów.
