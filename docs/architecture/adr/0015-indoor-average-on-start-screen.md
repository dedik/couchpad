# ADR 0015 — Średnia temperatura w domu na ekranie startowym

[English](0015-indoor-average-on-start-screen.en.md)

- **Data:** 2026-11-07
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0011](0011-weather-on-start-screen.md)

## Problem

Lewa kolumna ekranu startowego w poziomie miała dużo pustego miejsca pod pogodą. W każdym
pomieszczeniu jest czujnik temperatury i wilgotności (Zigbee), a pilot pokazywał wyłącznie
pogodę na zewnątrz.

## Decyzja

- **Jedna średnia, nie lista pomieszczeń.** Ekran startowy pokazuje „w domu”: średnią
  temperaturę i wilgotność z czujników pomieszczeń. Szczegóły pomieszczeń należą do zakładki Dom.
- **Liczy bridge.** Lista czujników jest w skrypcie `multimedia_remote_publish_weather`
  (`indoor_temperature`, `indoor_humidity`). Czujniki niedostępne są pomijane. NSPanele
  i termostaty się nie liczą — nagrzewają się same i zawyżają średnią.
- **Jedzie z pogodą.** Wiadomość na temacie `weather` dostaje opcjonalne pole
  `indoor: {temperature, humidity}`. To treść, nie stan urządzenia, więc obowiązują zasady
  pogody (ADR 0011): retained, pokazywana z pamięci, chowana po 6 godzinach. Zmiana addytywna:
  pilot bez obsługi pola go nie widzi, bridge bez pola — pilot chowa kafel.
- **Układ:** w poziomie godzina 96 px i data wyśrodkowane w lewej kolumnie, pod nimi kafle
  „na zewnątrz” i „w domu”; w pionie pigułka „W DOMU 23,8° 39%” pod pogodą. Temperatura
  w domu z dziesiątymi (pół stopnia w mieszkaniu ma znaczenie), na zewnątrz do pełnego stopnia.

## Skutki

- Wiadomość pogody wychodzi także po zmianie któregokolwiek czujnika pomieszczenia (kilka
  razy na kwadrans) — jest mała i retained.
- Przy okazji ikony kafli ekranu startowego stoją na środku wolnego miejsca nad nazwą.
