# Specyfikacja UX — TV i Logitech Z906

[English](tv-and-z906-ui.en.md)

## Status

To jest zatwierdzona specyfikacja projektowa v1 dla natywnej implementacji LVGL. Źródłem prawdy dla danych pozostaje `DeviceSnapshot` z wersjonowanego MQTT; przykładowe wartości w makiecie nie są stanem rzeczywistych urządzeń.

## Wspólna nawigacja

- Rozdzielczość docelowa: 480 × 800, portret. Orientacja pozioma 800 × 480 (ustawienie Ekran → Orientacja) ma osobne układy według makiety `docs/ux/mockup-landscape` — [ADR 0013](../architecture/adr/0013-landscape-orientation.md).
- Na dole obu widoków znajduje się wspólny, stale dostępny pasek z równymi zakładkami `TV` oraz `GŁOŚNIKI`.
- Aktywna zakładka jest wyraźnie zaznaczona. Dotknięcie przełącza bezpośrednio między ekranami, bez ekranu Home i bez potwierdzenia.
- Pasek styka się z dolną krawędzią obudowy wizualnej ekranu. Ma pozostać dyskretny odstęp między nim a kontrolkami znajdującymi się wyżej; nie może sprawiać wrażenia, że jest ich częścią.
- Wszystkie przyciski zasilania (TV i Z906 w nagłówku TV oraz Z906 na ekranie głośników) używają jednej konwencji: koło 48 px z polem dotyku 64 px; włączone — miękkie bursztynowe tło, bursztynowa ikona i cienka obwódka, wyłączone — neutralne tło i przygaszona ikona. Pełnego wypełnienia nie używamy (od 2026-11-06), bo dominowało nagłówek. Czerwony jest zarezerwowany dla wyciszenia i błędów, nigdy nie oznacza stanu zasilania.
- Zmiana zakładki nie odczytuje danych z sieci. Pokazuje aktualny lokalny `AppState`, który został uprzednio potwierdzony snapshotem.

## Ekran TV

### Kompozycja

1. Nagłówek: `LG OLED`, lokalizacja `SALON`, mały przełącznik zasilania TV i niewielki przełącznik zasilania Z906 obok.
2. Jeden rząd z dwoma wyborami obok siebie, bez osobnych dużych kart z napisami „ŹRÓDŁA” i „TRYB OBRAZU”:
   - po lewej bieżące źródło, po prawej bieżący tryb obrazu, każde jako jeden segment;
   - dotknięcie otwiera podstronę z listą opcji (cały wiersz listy jest polem dotyku).
3. Jeden rząd pięciu kafelków: cztery skróty kanałowe, logo-only (Polsat, TVN, TVP 1, TVP 2), i na końcu kafelek z ikoną listy, który otwiera podstronę „Kanały”: wszystkie kanały tunera w kolejności numerów, w wierszu numer i nazwa, bez strzałek. Bez kolorowych teł dodawanych do logo TVP.
4. Centralnie osadzony D-pad: Góra, Dół, Lewo, Prawo i `OK` dokładnie w środku. Po bokach pionowe kołyski jak na pilocie LG: `VOL` po lewej (głośność **Z906**, krok 2, z limitem z ustawień, powtarza się przy przytrzymaniu) i `P` po prawej (program +/-, klawisze pilota LG). Należy zachować pustą, oddechową przestrzeń między D-padem a przyciskami dolnymi.
5. Dwa przyciski akcji: `Cofnij` i `Ustawienia`.
6. Wspólny dolny pasek zakładek.

### Dane i zachowanie

- Dane TV pochodzą wprost z serwera Glasshouse na telewizorze ([ADR 0008](../architecture/adr/0008-tv-via-glasshouse.md), [integracja](../integrations/tv.md)).
- Źródła są dynamiczne: Glasshouse przekazuje listę wejść z ich nazwami z ustawień TV oraz aktualnie wybrane. Nie kodować listy na stałe.
- Tryb obrazu jest dynamiczny i może zmieniać się w zależności od materiału. Lista pokazuje tylko tryby, które TV teraz przyjmie. Nazwy typu „Dolby Vision Cinema Bright” kończy w segmencie wielokropek. Nie kodować nazw trybów na stałe.
- Po wyborze z listy segment pokazuje wybraną nazwę z pulsującą obwódką, aż TV potwierdzi zmianę.
- Nie wyświetlać dodatkowych fioletowych chipów ani tekstów typu `HDMI 1` / `Standard` poza samymi kontrolkami wyboru — wybrany stan jest już widoczny w kontrolce.
- Skróty kanałów przełączają wprost na kanał (Glasshouse stroi go i sam wyciąga Live TV na wierzch). Kanał wybiera się po nazwie z listy tunera, nie po zaszytym identyfikatorze.
- `Menu TV` otwiera szybkie ustawienia LG (klawisz ustawień pilota).
- Logo muszą być czytelne na ciemnym tle i mieć przezroczyste tło. Nie dodawać własnego niebieskiego ani pomarańczowego tła pod TVP 1/TVP 2.

## Ekran Logitech Z906 — Soundstage

To jedyny zaakceptowany kierunek ekranu dźwięku. Nie redukować go do małej karty.

1. Nagłówek wyłącznie `Z906`, `SALON` i **jeden** przycisk zasilania Z906. Na ekranie głośników nie może pozostać nagłówek TV ani drugi włącznik.
2. Wiersz informacji: bieżące źródło i efekt, np. `OPTICAL 1 · STEREO 2.1`.
3. Duży centralny pierścień głośności z liczbą procentową oraz dyskretną dekoracją orbitalną. Wyciszenie jest małym okrągłym przyciskiem z boku pierścienia.
4. Poziomy wybór źródła: `PC`, `RCA`, `O1`, `O2`, `COX`, `AUX`.
5. Sekcja „Scena dźwiękowa”: trzy pionowe suwaki `Center`, `Subwoofer`, `Tył`; bieżąca wartość 0–100 znajduje się nad właściwym suwakiem.
6. Wybór efektu: `Stereo 3D`, `2.1`, `4.1`, `Bez efektu`.
7. Wskaźnik `Decode` (dekodowanie 5.1; innego Z906 nie ma, więc bez liczby) i stan aktywności — dziś pastylka w nagłówku. To wskaźnik, nie przycisk.
8. Wspólny dolny pasek zakładek, odseparowany małą przerwą od stopki dekodowania.

### Wiązanie z danymi

| Element UI | Dane z snapshotu / bridge'a | Uwagi |
| --- | --- | --- |
| Zasilanie | `z906.power` | jawne `on` / `off`, bez zgadywania toggle |
| Wyciszenie | `z906.muted` | mały przycisk akcji |
| Pierścień | `z906.volume_percent` | 0–100, wysyłka max 10/s i finalna po puszczeniu; sterowanie obrotem względnym (niżej) |
| Źródło | `z906.source` + lista opcji | rzeczywiste opcje z HA |
| Center/Subwoofer/Tył | odpowiednie numery HA | 0–100, krok 2; przeciąganie względne (niżej) |
| Efekt | `z906.effect` + lista opcji | rzeczywiste opcje z HA |
| Decode 5.1 | stan binarny | tylko odczyt |

### Gest pierścienia głośności

- Głośność zmienia się **względnie**: o kąt, o jaki palec obróci się wokół środka pierścienia. Samo dotknięcie nie zmienia wartości — nie ma skoku do miejsca dotyku.
- Zgodnie z ruchem wskazówek zegara głośniej, przeciwnie ciszej; pełny obrót odpowiada 100 punktom, wynik zaokrąglany do kroku 2 i ograniczony do 0–100.
- Środek pierścienia (obszar z liczbą) jest strefą martwą — przy nim kąt jest niestabilny, więc ruch tam nie zmienia wartości.
- Polecenie `z906.volume.set` wysyłane jest dopiero po faktycznej zmianie wartości.

### Gest suwaków kanałów

- Center, Subwoofer i Tył też zmieniają się **względnie**: o pionowy ruch palca, w górę głośniej, w dół ciszej. Dotknięcie nie przestawia suwaka w miejsce palca.
- Polem chwytu jest cała kolumna kanału (etykieta, wartość i suwak), a nie tylko wąski tor suwaka.
- Około 120 px ruchu odpowiada pełnemu zakresowi 0–100 — wolniej niż sam tor suwaka, co ułatwia precyzyjne ustawienie z krokiem 2.
- `z906.level.set` wysyłane jest dopiero po faktycznej zmianie wartości; zasady oczekiwania na snapshot są takie jak dla pierścienia.

Dokładne obecne encje Home Assistant są w `../integrations/z906.md`. Firmware nie używa ich bezpośrednio; mapuje je bridge po stronie Home Assistant do kontraktu MQTT.

## Stany po wybudzeniu i podczas akcji

- Dotyk budzi pilot. Przez maksymalnie około 2–3 s może być widoczny lekki ekran łączenia.
- Ekran sterowania staje się interaktywny dopiero po nowym snapshocie w odpowiedzi na `remote.sync.request`.
- Po tapnięciu UI pokazuje stan oczekujący lokalnie, ale trwałą wartością jest dopiero odpowiedź snapshotu.
- Przy braku danych lub błędzie połączenia nie udawać aktualnego stanu.

### Wygląd stanów (makieta)

- **Łączenie z domem:** pełny ekran z symbolem domu w orbitach, tytułem i trzema krokami `Wi-Fi · MQTT · Synchronizacja`. Żadnych kafelków ani wartości urządzeń w tle.
- **Brak połączenia z domem:** ta sama kompozycja z ikoną braku sieci, krokiem zakończonym błędem, komunikatem, że zapamiętany stan nie jest pokazywany, oraz dużym przyciskiem `Ponów`.
- **Oczekiwanie:** tapnięta kontrolka pulsuje fioletowym obrysem; wybór przesuwa się dopiero po snapshocie. Suwak śledzi palec, a jego wartość pulsuje do potwierdzenia.
- **Brak potwierdzenia:** po limicie czasu kontrolka wraca do poprzedniej wartości i krótko sygnalizuje błąd czerwonym obrysem.
- **Urządzenie niedostępne:** panel TV lub Z906 jest wyszarzony i nieaktywny, na środku karta „Telewizor niedostępny” / „Z906 niedostępny”. Dolny pasek zakładek nadal działa; włącznik niedostępnego urządzenia w nagłówku jest wyłączony.

## Makieta referencyjna

Interaktywna wersja robocza jest w repozytorium:

```text
docs/ux/mockup/lg-tv-remote-ui.html
```

Otwiera się bezpośrednio w przeglądarce (ikony z Lucide przez CDN). Pod ekranem pilota jest panel „Symulacja stanów” do przełączania wybudzenia, błędu połączenia, niedostępności urządzeń i braku potwierdzenia akcji.

Nie kopiować jej HTML ani JavaScriptu do firmware. Służy do oceny wyglądu i interakcji; implementacja produkcyjna powstaje w C++/LVGL.
