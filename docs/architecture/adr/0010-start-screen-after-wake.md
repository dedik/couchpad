# ADR 0010 — Ekran startowy po włączeniu i wybudzeniu

[English](0010-start-screen-after-wake.en.md)

- **Data:** 2026-11-03
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0005](0005-fresh-state-after-wake.md)

## Problem

ADR 0005 każe po wybudzeniu pokazać wyłącznie nowy, potwierdzony stan. Dotąd znaczyło to
nakładkę „Łączenie z domem” nad całym pilotem aż do snapshotu: dotyk budził ekran, a potem
przez sekundę lub kilka nie było czego dotknąć. Do tego pilot wracał na ekran wybrany
w ustawieniach („Ekran po starcie”), a nie tam, dokąd użytkownik akurat zmierza.

## Decyzja

Po starcie i po każdym wybudzeniu pilot pokazuje **ekran startowy**: zegar, datę z nazwą
pokoju, cztery duże kafelki (Telewizor, Apple TV, Głośniki, Dom) i stan łącza z domem.
Wyłącznie wtedy — nawigacja go nie zna, pasek zakładek zostaje `TV | APPLE TV | DŹWIĘK | DOM
| WIĘCEJ`. Dotknięcie kafelka zamyka ekran startowy i otwiera ekran urządzenia.

Ekran startowy niczego nie steruje, więc wolno mu wyjść **od razu**, przed snapshotem.
Synchronizacja z ADR 0005 biegnie bez zmian, w tle. Kafelki mówią „łączenie…”, dopóki stan
nie przyjdzie, a potem jednym zdaniem streszczają świeży stan (np. „głośność 42 · Optical 1”)
— nigdy stan sprzed snu.

Nakładka łączenia zostaje, ale tylko na ekranach urządzeń: widać ją, gdy ktoś wejdzie na
urządzenie, zanim snapshot dotrze. Projekt: `docs/ux/mockup-pulpit` (README, „Ekran startowy”).

## Skutki

- Wybudzenie jest odczuwalnie szybsze: zanim palec trafi w kafelek, stan zwykle już jest.
- Ustawienie „Ekran po starcie” znika (`Pref::StartScreen`, `Pref::LastScreen`).
- Podstrony otwarte przed snem (lista wyboru, klima, ustawienia) zamykają się przy wybudzeniu
  — inaczej przykryłyby ekran urządzenia wybranego z kafelka.
- Stan błędu łącza widać już na ekranie startowym, z przyciskiem `Ponów`.
