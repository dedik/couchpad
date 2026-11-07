# ADR 0014 — Pomieszczenia i oświetlenie na ekranie Dom

[English](0014-rooms-and-lights.en.md)

- **Data:** 2026-11-07
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0002](0002-home-assistant-boundary.md), [ADR 0009](0009-climate-via-home-assistant.md)

## Problem

Ekran `DOM` miał kafelek klimatyzacji i cztery sceny, których kontrakt nigdy nie obsłużył
(polecenia chwilowe bez odbiorcy). Użytkownik chce sterować światłem: na razie lampą
w salonie — grupą `light` w Home Assistancie z trzech żarówek WiZ (jasność, barwa
2700–6500 K, 16 efektów) — a docelowo głównym światłem każdego pomieszczenia.
Makiety: `docs/ux/mockup-pulpit` (pion), `docs/ux/mockup-landscape` (poziom), wspólna
logika w `docs/ux/mockup-dom/rooms.js`.

## Decyzja

**Ekran.** Sceny znikają. Dom to siatka sześciu pomieszczeń (salon, kuchnia, przedpokój,
biuro, łazienka, sypialnia) jak ekran startowy, z pixelartową ikoną pokoju
(`tools/draw_room_icons.py`). W rogu kafelka lampka pokazuje stan głównego światła
i przełącza je dotknięciem. Klimatyzacja zostaje: w pionie jako wąski pasek nad siatką,
w poziomie jako pigułka w nagłówku. Dotknięcie kafelka otwiera pokój:

```text
Dom → pokój (urządzenia: światło, w salonie klima) → światło (jasność, barwa, efekt,
      żarówki) → pojedyncza żarówka albo lista efektów
```

W pokoju pasek pomieszczeń (te same ikony, bez lampek) przełącza pokój bez cofania.
W pionie stoi pod nagłówkiem. W poziomie pokój zajmuje cały ekran, a pasek zakładek
i pasek pomieszczeń są schowane w szufladach: przeciągnięcie od lewej krawędzi (pas
28 px, ruch ≥ 36 px) wysuwa zakładki, od prawej — pomieszczenia. Dotknięcie pasa przy
krawędzi też je wysuwa (uchwyty podpowiadają, że tam są), a dotknięcie przyciemnionego
tła albo przeciągnięcie do krawędzi chowa.

**Kontrakt.** Światła idą kontraktem `multimedia_remote/v1` jak klima: snapshot dostaje
sekcję `lights`, polecenia — akcje `light.*`. Zmiana jest addytywna, wersja zostaje `v1`.

- **Pilot nie zna encji.** Bridge ma tabelę `light_rooms` (pokój, identyfikator, etykieta,
  encja, żarówki) i wysyła krótkie identyfikatory (`lampa`, `lampa_lewa`). Polecenie
  z nieznanym identyfikatorem bridge pomija.
- **Główne światło może być przełącznikiem** (`switch.*`): wtedy `dimmable: false` i pilot
  pokazuje tylko wł./wył.
- **Jasność w procentach, barwa w kelwinach.** Zakres barwy i lista efektów przychodzą
  z encji. Pilot daje siedem kroków barwy równo w zakresie i dziesięć kroków jasności.
- **Efekt dotyczy całej grupy.** Pusty efekt to powrót do zwykłej bieli: bridge ustawia
  bieżącą barwę, bo WiZ wychodzi z efektu dopiero po zmianie barwy.
- **Jeden snapshot na gest.** Polecenie zmienia naraz grupę i żarówki, więc automatyzacja
  zmian świateł czeka 0,5 s (`mode: restart`). Bridge publikuje snapshot także po samym
  poleceniu — inaczej polecenie bez zmiany stanu czekałoby na potwierdzenie do limitu.

## Skutki

- Pokój bez wpisu w `light_rooms` pokazuje „brak światła”. Kolejne pomieszczenia to wpis
  w tabeli bridge'a i encje w wyzwalaczach automatyzacji — bez zmian w firmware
  (do trzech pomieszczeń ze światłem i czterech żarówek w grupie).
- Bez sekcji `lights` (bridge sprzed tej decyzji) kafelki mówią „czekam na stan”, reszta
  pilota działa jak dotąd.
- Zdarzenie klienta HA urosło o ok. 1,2 kB; jego kolejka leży w PSRAM.
- Duży kafelek klimy z ekranu Dom zniknął; podstrona klimy otwiera się z paska, pigułki
  albo z karty w salonie.
- Temperatury z termostatów w pomieszczeniach były tylko w makiecie i nie weszły do
  kontraktu.
