# ADR 0011 — Pogoda na ekranie startowym

[English](0011-weather-on-start-screen.en.md)

- **Data:** 2026-11-03
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0005](0005-fresh-state-after-wake.md), [ADR 0010](0010-start-screen-after-wake.md)

## Problem

Ekran startowy pokazuje zegar i cztery kafelki. Obok godziny jest miejsce na bieżącą pogodę,
a Home Assistant ją zna (`weather.forecast_home`, met.no). Pogoda zmienia się raz na kilkadziesiąt
minut, więc pytanie o nią przy każdym wybudzeniu marnowałoby ruch i pamięć.

## Decyzja

Bridge publikuje pogodę na **osobnym temacie retained**, jak listę kanałów (ADR 0008), a nie
w snapshocie stanu:

```text
multimedia_remote/v1/<remote_id>/weather       Home Assistant → pilot, QoS 1, retained
{ "condition": "partlycloudy", "temperature": 5.5, "night": true }
```

Publikuje ją przy zmianie warunku lub temperatury, przy wschodzie i zachodzie słońca i po
starcie HA — nie na żądanie pilota. `night` (słońce pod horyzontem, `sun.sun`) jest potrzebne,
bo HA ma nocny wariant tylko dla bezchmurnego nieba: `partlycloudy` o drugiej w nocy to
księżyc za chmurą, nie słońce.

Pogoda to **treść, nie stan urządzenia**. ADR 0005 zabrania pokazywać po wybudzeniu stan
urządzeń z pamięci, ale pogody to nie dotyczy: pilot trzyma ostatnią w pamięci i pokazuje ją
od razu, bez czekania na synchronizację. Po restarcie pilota broker oddaje ostatnią wartość sam,
bo temat jest retained. Pogoda starsza niż 6 godzin znika z ekranu zamiast udawać aktualną.

`condition` to identyfikator HA (`sunny`, `rainy`…); polską nazwę i ikonę dobiera pilot.

## Skutki

- Zero dodatkowych zapytań przy wybudzeniu; jedna wiadomość na zmianę pogody.
- Snapshot stanu nie rośnie — pogoda nie jedzie w kolejkach ani kopiach stanu.
- Encja jest na sztywno w bridge'u (`weather.forecast_home`).
