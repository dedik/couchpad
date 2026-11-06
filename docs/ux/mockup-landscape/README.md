# Makieta — orientacja pozioma 800 × 480

[English](README.en.md)

Klikalna makieta interfejsu pilota w poziomie. Na jej podstawie powstały układy poziome
w firmware ([ADR 0013](../../architecture/adr/0013-landscape-orientation.md)). Korzysta
z ikon pikselowych i logotypów z [`../mockup-pulpit/assets/`](../mockup-pulpit/assets/)
oraz ze zrzutów pionu z [`../../screenshots/`](../../screenshots/) do porównania, więc
potrzebuje serwera z katalogiem `docs` jako korzeniem:

```bash
cd docs && python3 -m http.server 4174
```

Potem `http://localhost:4174/ux/mockup-landscape/index.html`.

## Przełączniki nad makietą

| Przełącznik | Co pokazuje |
| --- | --- |
| Pasek | boczny pasek zakładek po lewej albo po prawej |
| Styl | ciemny i jasny |
| TV: wybór | źródło i tryb obrazu w nagłówku (firmware) albo w kolumnie |
| Z906: wybór | źródło i efekt w nagłówku (firmware) albo nad suwakami |
| Klima | warianty karty klimatyzacji na ekranie Dom; firmware ma „C szybkie” |
| Podstrona klimy | warianty podstrony; firmware ma „P1” |
| Kafle trybu | 3 × 2 albo duży kafel trybu po lewej; firmware: „duży = wybrany” |
| Zasilanie | pełne koło 64 px albo mniejsze 48 px (firmware) |
| porównaj z pionem | obok makiety zrzut tego samego ekranu w pionie |
| cele dotyku | obrys pól dotyku |

Makieta służy do oceny wyglądu i zachowania. Nie kopiuje się z niej HTML ani JavaScriptu
do firmware.
