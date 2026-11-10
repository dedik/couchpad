# ADR 0020 — Zakładka Pilot

[English](0020-remote-tab.en.md)

- **Data:** 2026-11-10
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0010](0010-start-screen-after-wake.md), [ADR 0013](0013-landscape-orientation.md), [ADR 0016](0016-android-app.md)

## Problem

Głównym zadaniem urządzenia jest pilot do telewizora, Apple TV i głośników, a codzienne
sterowanie było rozrzucone po trzech ekranach. Oglądając film przez Apple TV, trzeba było
przechodzić między gładzikiem (Apple TV), głośnością (Z906) i źródłem albo P± (TV).

## Decyzja

- **Nowy pierwszy ekran „Pilot”** (`ScreenId::Remote`, `components/ui/screens/screen_remote.cpp`;
  w aplikacji `ui/remote/RemoteScreen.kt`) z trzema blokami i tylko najważniejszymi kontrolkami:
  - **Telewizor:** wejścia jako segmenty z ikoną — telewizja i wejścia nazwane w ustawieniach
    TV; wejścia z domyślną nazwą („HDMI 2”) zostają na pełnym ekranie TV, chyba że są aktywne;
    **aktywne zawsze pierwsze** (`domain/tv_sources.hpp`); tryb obrazu jedną linią ‹ nazwa ›
    (strzałki — poprzedni / następny w kółko, nazwa — pełna lista, kropki — pozycja);
    Cofnij, P−, P+. W poziomie dodatkowo skróty kanałów i lista kanałów.
  - **Apple TV:** ten sam gładzik co na ekranie Apple TV, Menu, Home (stuknięcie, ×2,
    przytrzymanie) i Klawiatura; zasilanie w nagłówku bloku. Klawiatura wysuwa się sama także
    na tym ekranie, gdy Apple TV pokaże pole tekstowe.
  - **Głośność Z906:** −/+ o poziom wzmacniacza (przytrzymanie powtarza), wartość, wyciszenie.
    W pionie wejście dźwięku przy ikonie kolumny. W **poziomie** (kolumna 150 px, makieta
    `docs/ux/mockup-remote-volume.html`, G2) nagłówek jak przy TV i Apple TV (ikona, „Z906”,
    Decode 5.1), wejście jako kafel z bieżącą wartością (stuknięcie — lista z ekranu Głośniki),
    cztery efekty jawnie 2 × 2 (3D, 2.1, 4.1, Brak) i −/+ obok siebie w jednym rzędzie.
  Bez logotypów aplikacji, poziomów kanałów Z906, efektów i Decode — te zostają na pełnych ekranach.
- **Bez nagłówka powłoki** na tym ekranie. W **poziomie** także bez bocznego paska zakładek:
  chowa się w szufladzie przy lewej krawędzi, jak w głębi zakładki Dom (pas chwytu 28 px,
  uchwyt 5 × 64 px, przeciągnięcie ≥ 36 px albo stuknięcie). Zyskane 88 px idą na klawisze
  68 px (`kTouch`) i skróty kanałów (makieta `docs/ux/mockup-remote-panel.html`, wariant L3).
  W **pionie** bez nagłówków bloków Telewizor i głośności (wariant P2): segmenty wejść i ikona
  kolumny mówią same za siebie, zwolnione miejsce idzie na klawisze 68 px.
- **Pasek zakładek ma sześć pozycji**, Pilot pierwszy. W poziomie zakładka ma 72 px wysokości
  (6 × 72 + 5 × 8 = 472 ≤ 480; było 80), w pionie ok. 71 px szerokości (było 86) — nadal
  ≥ 68 px celu dotyku.
- **Ekran startowy:** na skrzyżowaniu kafelków okrągły przycisk Pilota (132 px, obrys
  w akcencie, ikona 72 px, obwódka 12 px w kolorze tła wycina łuk w narożnikach). Górny rząd
  kafelków ma nazwę i stan u góry, żeby przycisk nic nie zasłaniał. Lewa / górna część startu
  bez zmian.
- **Kontrakt MQTT bez zmian.** Pilot wysyła te same zamiary co pełne ekrany (`tv_*`,
  `appletv_*`, `z906_step_volume`, `SetMuted`).
- Zakładka powstała najpierw w aplikacji (odstępstwo od „panel jest wzorcem”); ten ADR
  przenosi ją na panel, więc obie wersje znów są zgodne (z różnicą rozmiarów — panel ma
  ≈ 8,6 px na mm, więc klawisze są większe niż w aplikacji).

## Skutki

- `tools/remote.py`: nowe współrzędne zakładek w pionie i zrzut `15-remote` w `tour`.
- Nowe ikony pixel-art: `tools/draw_remote_icons.py` (`remote`, `key_*`), w firmware przez
  `tools/gen_assets.py` (`icon_remote`, `icon_remote_hub` 72 px, `icon_key_*` 24 px,
  `icon_tv_small`, `icon_appletv_small` 24 px).
- Ekran TV pozostaje pełnym widokiem (krzyżak, VOL, Menu TV); Pilot nie zastępuje pełnych ekranów.
