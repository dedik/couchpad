# Pulpit CRT — wariant pikselowy

[English](README.en.md)

Trzeci kierunek interfejsu, obok [`../mockup/`](../mockup/) i [`../mockup-pulpit/`](../mockup-pulpit/).
Żadnego z nich nie zastępuje.

To **ten sam system co Pulpit**: te same ekrany, te same nazwy klas, ta sama logika stanów i ta sama
obsługa gestów. Zmienia się wyłącznie warstwa wyglądu — i to jest zarazem sprawdzian, czy struktura
Pulpitu w ogóle się trzyma. Okazało się, że tak: wystarczyła podmiana arkusza stylów.

| Plik | Co zawiera |
| --- | --- |
| `index.html` | makieta: pięć ekranów, pełna symulacja stanów |
| `system.html` | galeria komponentów w tym motywie |
| `pixel.css` | cała różnica względem Pulpitu |
| `pixel-icons.js` | ikony zakładek, wspólne z Pulpitem |

## Jak obejrzeć

```bash
cd docs/ux && python3 -m http.server 4173
```

Potem `http://localhost:4173/mockup-pixel/index.html`.

## Zasady tego motywu

1. **Żadnych zaokrągleń i żadnych rozmytych cieni.** Jedyny wyjątek to pierścień głośności, bo
   pierścień musi być pierścieniem.
2. **Fazka zamiast cienia.** Dwa piksele światła od góry i lewej, dwa piksele cienia od dołu i
   prawej. Naciśnięty element ma fazkę odwróconą — to cała animacja, jakiej potrzebuje.
3. **Dithering zamiast gradientu.** Szachownica 2 px tam, gdzie Pulpit miał płynne przejście.
4. **Siatka 4 px.** Wszystkie odstępy i wymiary są jej wielokrotnością.
5. **Linie kineskopu** na całym ekranie: ciemny pasek co dwa piksele, ledwie widoczny.
6. **Mruganie zamiast pulsowania.** Stan oczekiwania miga skokowo, bo pixel art nie zna półcieni.

Zmiany, które nie są samą dekoracją:

- **Pierścień głośności jest podzielony na trzydzieści działek.** Wartość da się policzyć wzrokiem,
  a nie tylko oszacować. Słupki kanałów też mają poprzeczki.
- **Plakaty pobierane są w rozdzielczości 50 × 75** i powiększane bez wygładzania, więc są
  prawdziwym pixel artem, a nie fotografią z filtrem.
- **Pasek telemetrii** pod nagłówkiem pokazuje stan MQTT, numer rewizji, siłę sygnału i czas
  odpowiedzi. W Pulpicie te dane siedzą na ekranie diagnostycznym; tutaj są stale na wierzchu, bo
  taki jest sens wariantu dla geeków. Numer rewizji rośnie po każdym potwierdzonym snapshocie.

## Krój pisma i pułapka, w którą wpadłem

**Silkscreen** na etykiety, **VT323** na tekst i odczyty. Oba mają komplet polskich znaków.

Pierwsza wersja miała etykiety w rozmiarach 10 i 12 px i polskie znaki się rozpadały — `GŁOŚNOŚĆ`
wyglądało jak `G.OŚNOŚĆ`. Przyczyną nie był brak glifów, tylko skala: **krój pikselowy wolno
powiększać wyłącznie całkowitą liczbę razy**. Silkscreen jest rysowany na siatce 8 px, więc wolno
mu dać 8, 16 albo 24 — przy 10 px wychodzi skala 1,25 i piksele lądują między pikselami ekranu.
Znaki z ogonkami i kreskami rozpadają się pierwsze, bo ich detale są najcieńsze.

Z tego samego powodu w tym motywie nie ma `letter-spacing`: dodatkowy odstęp rozsuwa znaki poza
siatkę.

## Co to znaczy dla firmware

Ten wariant jest **tańszy w rysowaniu** niż gładki Pulpit, nie droższy:

- Krój pikselowy w swoim rozmiarze to 1 bit na piksel i zero antyaliasingu — ułamek pamięci,
  jakiej potrzebuje Ubuntu przy 4 bitach.
- Fazki to dwa prostokąty na krawędziach, a nie cień z rozmyciem, które LVGL liczy programowo.
- Dithering to kafelek 4 × 4 powtarzany sprzętowo, a nie gradient.
- Brak zaokrągleń oznacza brak wygładzania narożników.

Drożej wychodzi tylko pierścień: trzydzieści działek to trzydzieści łuków zamiast jednego. W LVGL
robi się to jednym `lv_arc` z tłem w postaci kafelka z przerwami albo trzydziestoma `lv_obj`
ustawionymi raz przy budowie ekranu.

## Czego ten wariant nie zmienia

Kontraktu MQTT, modelu danych, zasady świeżego stanu po wybudzeniu, zakresów 0–100 z krokiem 2 ani
zakazu zgadywania encji. Wszystkie zastrzeżenia z [README Pulpitu](../mockup-pulpit/README.md)
obowiązują tu tak samo, łącznie z tym, że przyjęcie tego kierunku wymaga ADR-ów.
