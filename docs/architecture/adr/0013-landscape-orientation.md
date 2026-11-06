# ADR 0013 — Orientacja pozioma

[English](0013-landscape-orientation.en.md)

- **Data:** 2026-11-06
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0003](0003-single-ui-owner.md), [ADR 0012](0012-light-and-dark-theme.md)
- **Zmienia:** wymóg dolnych zakładek z `AGENTS.md` i rozdzielczość z `docs/ux/tv-and-z906-ui.md`

## Problem

Interfejs był projektowany wyłącznie w pionie, 480 × 800, natywnej orientacji panelu.
Pilot ma też stać na stoliku albo w podstawce, a wtedy wygodniejszy jest poziom: klawiatura
dostaje szersze klawisze, a ekrany z kilkoma strefami (Z906, ustawienia) mieszczą je obok
siebie zamiast jedna pod drugą.

Panel ST7701 pracuje przez DPI i nie obraca obrazu sam (`swap_xy is not supported`). Płytka nie
ma żyroskopu ani akcelerometru, więc pilot nie wie, jak jest trzymany.

## Decyzja

**Orientacja jest ustawieniem użytkownika, zmienianym wyłącznie w Ekran → Orientacja, i
obowiązuje od restartu.**

- `storage::Pref::Orientation`: 0 pion, 1 poziom (obrót o 90°), 2 poziom odwrócony (270°).
  `main` czyta ją przed `board::start(rotation)`.
- Obrót robi `esp_lvgl_port` (`sw_rotate`) na każdym fragmencie ramki przy odświeżaniu,
  procesorem (`lv_draw_sw_rotate`). W pionie obrót jest wyłączony, więc nie ma drugiego bufora
  ani kosztu. Dotyk GT911 przelicza LVGL według obrotu ekranu.
- Ekrany są budowane raz (zasada z `ui-architecture.md`), więc zmiana orientacji to restart,
  nie przebudowa w locie. `theme::landscape()` mówi ekranom, który układ zbudować.
- Każdy ekran ma dwa układy budowy i **jeden** kod wiązania ze stanem (`render_*`). Układ
  poziomy składa te same widżety inaczej; nie ma osobnych struktur ani duplikatów danych.
- Poziom według makiety `docs/ux/mockup-landscape`:
  - zakładki w pionowym pasku z lewej (88 px) zamiast dolnego paska;
  - TV: źródło i tryb obrazu w jednym rzędzie w nagłówku, kanały oraz Cofnij i Menu TV po
    lewej, krzyżak z kołyskami po prawej;
  - Apple TV: aplikacje i trzy akcje po lewej, gładzik po prawej (bez karty odtwarzania);
  - Z906: źródło i efekt w nagłówku obok pastylki Decode, tarcza głośności po lewej, trzy
    kanały po prawej z torem 300 px;
  - Dom: karta klimatyzacji po lewej z zasilaniem, trybem (lista) i −/+ (jedno polecenie
    0,7 s po ostatnim stuknięciu), sceny 2 × 2 obok;
  - Klimatyzacja: tarcza po lewej, tryby jako kafle z ikonami (wybrany duży po lewej, reszta 2 × 2 obok) i nawiew w rzędzie po prawej,
    z odstępem pod zasilaniem;
  - Ustawienia: lista z lewej, wybrana podstrona zadokowana obok niej;
  - listy wyboru jako panel 380 px przy prawej krawędzi nad przyciemnionym ekranem;
  - klawiatura z Anuluj i Zapisz w rzędzie pola tekstowego.
- `tools/remote.py` dostaje obrót w nagłówku zrzutu i obraca obraz; zdalny dotyk przyjmuje
  współrzędne logiczne.

## Odrzucone

- **Automatyczny obrót** — brak czujnika położenia.
- **Przebudowa interfejsu w locie** — usuwanie i odtwarzanie setek obiektów oraz wskaźników
  w `Context`; restart jest prostszy i trwa kilka sekund.
- **Obrót przez PPA** (`CONFIG_LVGL_PORT_ENABLE_PPA`) — pierwsza wersja. Przy przewijaniu
  listy w poziomie zadanie LVGL zawieszało się na stałe: czekało na PPA z trzymaną blokadą,
  więc nie zgłaszał się watchdog, a zrzut z `remote.py` nie dostawał ramki. Odtworzone 2/2,
  bez PPA 0 zawieszeń przy tych samych gestach (2026-11-06). Przyczyny w PPA nie ustalono;
  powrót do niego wymaga najpierw jej znalezienia.

## Skutki

- Każdy nowy ekran lub podstronę projektuje się w obu orientacjach. W poziomie treść ma
  672 × 383 px (po pasku zakładek i nagłówku), w pionie 432 × 623 px.
- Dolne zakładki `TV | GŁOŚNIKI…` są obowiązkowe w pionie; w poziomie ich rolę pełni boczny
  pasek z tymi samymi zakładkami.
- `tour` w `remote.py` zna tylko współrzędne pionu.
- W poziomie sprawdzone zdalnymi stuknięciami z `remote.py`; dotyk palcem i obrót o 270°
  czekają na potwierdzenie na płytce.
