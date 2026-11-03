# Pulpit — alternatywny kierunek UI

[English](README.en.md)

Drugi, niezależny projekt interfejsu pilota. **Nie zastępuje** zatwierdzonej specyfikacji z
[`../tv-and-z906-ui.md`](../tv-and-z906-ui.md) ani makiety w [`../mockup/`](../mockup/) — obie zostają
bez zmian. To materiał do porównania i decyzji.

Ten sam system ma też wariant pikselowy: [`../mockup-pixel/`](../mockup-pixel/). Różni się wyłącznie
arkuszem stylów, co jest przy okazji dowodem, że struktura opisana niżej trzyma się bez względu na
wygląd.

| Plik | Co zawiera |
| --- | --- |
| `index.html` | makieta: cztery ekrany, pełna symulacja stanów |
| `system.html` | system projektowy: tokeny, komponenty, stany, mapowanie na LVGL |
| `pulpit.css` | jedyne źródło stylu; obie strony korzystają z tego samego pliku |
| `pixel-icons.js` | ikony zakładek w pixel art, wspólne dla obu stron |

## Jak obejrzeć

Pliki są rozdzielone, więc wymagają serwera (`file://` nie wczyta arkusza i logotypów):

```bash
cd docs/ux && python3 -m http.server 4173
```

Potem `http://localhost:4173/mockup-pulpit/index.html` oraz `.../system.html`.

## Problem, który ten kierunek rozwiązuje

Zatwierdzona makieta projektuje **dwa ekrany**. Dokumentacja architektury przewiduje docelowo sześć
(Home, TV, Z906, Home Assistant, Settings, Diagnostics). Przy dokładaniu kolejnych pojawiają się trzy
kłopoty: dolny pasek z dwiema zakładkami nie ma gdzie rosnąć, nagłówek ekranu zmienia znaczenie
zależnie od widoku, a każdy ekran dostaje własne odcienie i rozmiary.

Pulpit odpowiada na to jedną powłoką i jednym zestawem komponentów.

## Zasady

**1. Trzy strefy, zawsze te same.** Nagłówek 64 px (tożsamość ekranu + zasilanie jego urządzenia),
treść, pasek zakładek 80 px z samymi ikonami (od 2026-11-03; wcześniej 88 i 96 px, z podpisami
i paskiem statusu). Nad nagłówkiem 5 px odstępu. Nowy ekran wypełnia wyłącznie strefę środkową.

**2. Nagłówek należy do bieżącego ekranu.** Pokazuje nazwę, pomieszczenie i zasilanie tylko tego
urządzenia, które widać. Na ekranie dźwięku nie ma włącznika TV, a na ekranach bez urządzenia
(Dom, Więcej) nie ma włącznika w ogóle. Znika problem „dwa włączniki na jednym pasku”.

**3. Stała liczba slotów nawigacji.** Pięć zakładek: `TV`, `APPLE TV`, `DŹWIĘK`, `DOM`, `WIĘCEJ`.
Przy 480 px szerokości daje to 86 px na zakładkę, czyli wciąż wygodny cel dotyku. **To jest granica** —
szósty ekran nie dostaje własnej zakładki, tylko wiersz w `WIĘCEJ`. Pasek nigdy nie zmienia układu.

**4. Jeden akcent.** Bursztyn oznacza „aktywne”: włączone zasilanie, wybrana zakładka, wypełnienie
wartości, przycisk OK. Czerwony ma jedno znaczenie — wyciszenie i błąd. Zielony — potwierdzona
dostępność. Trzy kanały Z906 mają ten sam kolor; rozróżniają je podpisy, nie barwy.

**5. Rzeczywiste piksele.** Makieta renderuje się w 480 × 800, nie w skali. Panel 4,3″ ma ok.
8,6 px/mm, więc minimalny cel dotyku to 68 px (≈ 8 mm). Przy makiecie skalowanej do ~368 px łatwo
zaprojektować przyciski, które na sprzęcie mają 4 mm i są nietrafialne.

**6. Pięć stanów każdego sterowania.** Spoczynek, wybrany, oczekiwanie na snapshot, brak
potwierdzenia, brak świeżego stanu. Wynikają wprost z ADR 0005 i kontraktu MQTT: dotknięcie nigdy
nie przesuwa zaznaczenia samo z siebie — robi to dopiero snapshot.

**7. Wartości ciągłe zmieniają się względnie.** Tarcza reaguje na obrót palca, kolumny na ruch
w pionie. Dotknięcie nie przestawia wartości w miejsce palca.

## Ekran Apple TV

Zbudowany wyłącznie z komponentów systemu, z trzema nowymi:

- **Gładzik** zamiast krzyżaka, bo tak działa prawdziwy pilot Apple TV. Jest **kołem o średnicy
  240 px**, wyśrodkowanym tak jak krzyżak na ekranie TV — ta sama sylwetka dla tej samej roli.
  Kształt wynika z gestu: pas szeroki na cały ekran dawał proporcję 2,7 : 1, czyli trzy razy mniej
  drogi dla przesunięcia w pionie niż w poziomie. Koło daje obu kierunkom tyle samo miejsca, a
  strefę wyznacza odległość od środka i kąt, nie prostokąty. Powierzchnia jest jednolita: nie ma na
  niej wypełnionego przycisku, bo akcent w tym systemie oznacza stan, a nie akcję — a kolorowy blok
  na środku pola do przesuwania palcem przeczy samemu gestowi. Podświetla się dopiero to, co
  właśnie naciskasz. `Menu` i `Home` stoją po bokach koła, zamiast zajmować osobny wiersz.
- **Karuzela nowości** z plakatami 100 × 150, przewijana poziomo z zatrzymywaniem na pozycji.
  Nad nią przełącznik `Nowe filmy | Nowe seriale`. Dwie osobne karuzele zajęłyby ok. 360 px z 588
  dostępnych, a schowanie ich za przyciskiem odebrałoby ekranowi to, co w nim najlepiej wygląda.
  Tytuł leży na plakacie pod przyciemnieniem, więc nie zabiera wysokości. Dotknięcie otwiera okno
  szczegółów z opisem i przyciskiem odtwarzania.
- **Okno szczegółów** — pierwszy komponent nakładkowy w systemie, oparty na tym samym przyciemnieniu
  tła co ekran wybudzania.

Skróty aplikacji (Infuse, YouTube, WP Pilot) działają jak skróty kanałów na ekranie TV: na razie
wizualnie, docelowo uruchamiają aplikację przez bridge.

### Biblioteka i plakaty

Metadane pozycji idą osobnym tematem MQTT, a **plakaty pobiera się po HTTP** — snapshot stanu jest
retained i nie wolno go obciążać listą filmów. Emby skaluje obraz po swojej stronie (`maxWidth`),
więc pilot pobiera od razu docelowe 106 × 160: ok. 10 KB jako JPEG, ok. 36 KB po dekompresji.
Pobieranie i dekodowanie wykonuje zadanie robocze, nigdy zadanie UI.

**To wymaga osobnego ADR-u**, bo rozszczepia regułę z ADR 0005. Stan urządzeń musi być świeży po
każdym wybudzeniu. Biblioteka jest treścią, nie stanem — wolno ją trzymać w pamięci podręcznej i
pokazać od razu po wybudzeniu, bez czekania na sieć. Bez zapisania tej różnicy ktoś potraktuje
jedno jak drugie.

Klucz API do Emby nie trafia do repozytorium — idzie do konfiguracji urządzenia w NVS.

## Ekran startowy

Po starcie i po każdym wybudzeniu pilot pokazuje **ekran startowy** zamiast ostatniego
ekranu urządzenia — **wyłącznie wtedy**. Nawigacja go nie zna: dotknięcie kafelka zamyka go
do następnego wybudzenia, a pasek zakładek zostaje bez zmian (`TV | APPLE TV | DŹWIĘK | DOM |
WIĘCEJ`). Pełny ekran, nad nagłówkiem i paskiem zakładek:

- u góry zegar 56 px i data z nazwą pokoju;
- cztery kafelki 2 × 2 (ok. 210 × 270 px): Telewizor, Apple TV, Głośniki, Dom. Ikona to
  sprite zakładki w skali 4 × (96 px, każdy piksel siatki na 4 × 4 piksele ekranu), pod nią
  nazwa 26 px i jedno zdanie stanu z kropką: akcent = włączone, szara = wyłączone, pulsująca =
  łączenie, czerwona = brak świeżego stanu;
- na dole stan łącza z domem i `Ponów` po błędzie;
- po prawej od zegara pogoda z HA (ADR 0011): ikona pixel art 64 px, temperatura do pełnego
  stopnia i nazwa warunku. To treść, nie stan, więc pokazuje się od razu z pamięci; w nocy
  `partlycloudy` ma księżyc zamiast słońca.

**Po co.** Ekran sterowania nie może pokazać stanu z pamięci (ADR 0005), więc dziś po
wybudzeniu blokuje go nakładka „Łączenie z domem”. Ekran startowy niczego nie steruje, więc
wychodzi od razu, a synchronizacja biegnie w tle; zanim palec trafi w kafelek, stan zwykle
już jest. Wejście na urządzenie przed snapshotem pokazuje nakładkę jak dotąd.

Przyjęcie w firmware wymaga ADR-u: zmienia szczegół ADR 0005 (nakładka tylko na ekranach
urządzeń) i zastępuje ustawienie „Ekran startowy” — ekranem po wybudzeniu jest zawsze start.

## Ekran Dom i klimatyzacja

Ekran `DOM` zaczyna się od **kafelka urządzenia**: 432 × 168, bez żadnego sterowania. Pokazuje tylko
to, co trzeba wiedzieć, zanim się w niego wejdzie — tryb pracy i temperaturę docelową (duża liczba),
pod spodem temperaturę w pokoju. Włączona klima ma akcent na znaku i nazwie trybu; wyłączona jest
szara; brak świeżego stanu zamienia wartości na kreskę i obrysowuje kafelek na czerwono. Dotknięcie
otwiera podstronę. Skróty (Światło, Rolety, Klimat, Kamera) zniknęły: Klimat zastąpił kafelek, a
reszta nie miała jeszcze żadnej treści.

**Podstrona klimatyzacji** to `widgets::make_page`: pełny ekran nad paskiem zakładek, w nagłówku
`Cofnij` po lewej i zasilanie klimy po prawej, oba 64 px. Treść odpowiada karcie termostatu w HA dla
`climate.living_room_ac`:

- **Tarcza 300 px**, łuk 270° na zakres 16–31 °C, krok 0,5. Zmienia się obrotem palca, względnie, tak
  jak głośność. Pod łukiem `−` i `+` po 72 px. Kilka stuknięć to jedno polecenie: wysyłka idzie 0,7 s
  po ostatnim dotknięciu, wartość do tego czasu pulsuje akcentem. W trybie wentylatora i po
  wyłączeniu tarcza gaśnie, bo temperatura docelowa wtedy nie działa.
- **Tryb pracy** — pięć segmentów z ikoną: Auto, Chłodzenie, Grzanie, Osuszanie, Wentylator.
  Wyłączenie nie ma segmentu, to robota przycisku zasilania; wybór trybu przy wyłączonej klimie
  zarazem ją włącza (`climate.set_hvac_mode`), a zasilanie wraca do ostatniego trybu.
- **Moc nawiewu, żaluzje pion i poziom** — trzy kafelki z bieżącą wartością. Moc ma osiem biegów
  (`fan_modes`: Auto, Cicha, Niska, Średnio niska, Średnia, Średnio wysoka, Wysoka, Maksymalna),
  więc wybór idzie z listy w oknie od dołu, jak źródło i tryb obrazu TV: jedna kolumna po 68 px,
  każdy bieg z ikoną, jak w HA. Żaluzje mają krótsze listy bez ikon. Okno zamyka się dopiero po
  potwierdzeniu snapshotem.

Jeden akcent zostaje: chłodzenie nie jest niebieskie, a grzanie pomarańczowe — tryb rozróżnia ikona
i podpis. Zakres, krok i listy opcji makieta ma wpisane na sztywno; w firmware przyjdą z atrybutów
encji (`min_temp`, `max_temp`, `target_temp_step`, `hvac_modes`, `fan_modes`, `swing_modes`,
`swing_horizontal_modes`). To wymaga nowej sekcji w kontrakcie MQTT, a więc ADR-u.

W firmware: `ui/screens/screen_climate.cpp` i `widgets::Thermostat`. Odczyt 64 px ma własną mapę
kroju (cyfry, przecinek, pauza), znak stopnia idzie mniejszym krojem obok. Dwie różnice wobec
makiety: segmenty trybów są tekstowe, bo ikon funkcyjnych z makiety jeszcze nie ma, a lista mocy
to podstrona z wierszami, jak lista źródeł TV, zamiast okna od dołu.

## Ikony zakładek w pixel art

Zakładki mają kolorowe sprite'y w stylu retro. Przycisk w panelu symulacji przełącza je z powrotem
na zwykłe ikony liniowe, gdyby przyszło porównać.

Ikony są autorskie, nie z gotowego pakietu — żaden nie ma znaku pasującego do Apple TV obok
pozostałych czterech, a mieszanie źródeł rozbija spójność rodziny.

**Od 2026-11-03 to siatka 48 × 48** (`assets/pixel/*.png`), rysowana skryptem
`tools/draw_icons.py`: każdy materiał ma rampę sześciu odcieni (światło z lewej góry, cień
z prawej dołu), do tego refleksy, ciemny kontur i półprzezroczysty cień pod spodem — styl
wzorowany na szczegółowych zestawach pixel art. Na zakładce ikona idzie w skali 1 × (48 px),
na kafelku ekranu startowego w 2 × (96 px), zawsze bez wygładzania. Poprawia się ją w skrypcie
(i uruchamia go ponownie) albo wprost w PNG w edytorze pikseli. Dawne siatki 24 × 24 w
`pixel-icons.js` zostały tylko jako zapas dla galerii komponentów.

Motywy: telewizor kineskopowy z anteną, płaski dekoder z trójkątem odtwarzania, kolumna głośnikowa
z niskotonowym i wysokotonowym, dom z oświetlonymi oknami oraz siatka trzy na trzy. Dekoder jest
celowo niski i szeroki, żeby nie mylił się z telewizorem. Wspólna paleta i akcent systemu w kilku
detalach wiążą je w jedną rodzinę.

**Podpisy zostają.** Ryzykiem nie jest styl ikon, tylko usunięcie tekstu: przy pięciu zakładkach,
z których dwie dotyczą telewizora, sama ikona nie wystarczy do rozróżnienia.

Ponieważ sprite ma własne kolory, wybranej zakładki nie znaczy już barwa ikony, tylko jej tło.

Dla firmware to zresztą ułatwienie: siatka 48 × 48 staje się obrazem indeksowanym — w zakładce
w skali 1 ×, na kafelku ekranu startowego powiększonym dwukrotnie metodą najbliższego sąsiada,
bez antyaliasingu — taniej niż font ikon.

## Krój pisma

**Ubuntu**, w trzech odmianach: 400, 500 i 700. Projekt nie używa żadnej innej grubości, więc ani
przeglądarka, ani firmware nie muszą niczego pogrubiać sztucznie. W makiecie krój przychodzi z
Google Fonts; to jedyna zewnętrzna zależność obok biblioteki ikon.

Dwie rzeczy przemawiają za nim w tym projekcie:

- **Wszystkie dziesięć cyfr ma tę samą szerokość.** Sprawdzone pomiarem: 26,13 px przy 46 px w
  odmianie 700, identycznie dla każdej cyfry. Odczyt głośności i poziomy kanałów nie będą więc
  skakać przy zmianie wartości i nie trzeba do nich osobnego kroju o stałej szerokości.
- **Pełne polskie znaki**, więc nic nie trzeba dorysowywać.

### Co to znaczy dla firmware

Zmiana kroju jest tania, bo LVGL i tak nie używa plików TTF w czasie działania: `lv_font_conv`
zamienia krój na mapy bitowe **osobno dla każdego rozmiaru**. Podmiana kroju to przegenerowanie
tych map, bez zmian w kodzie ekranów.

Koszt to pamięć programu. Projekt używa sześciu rozmiarów (11, 13, 15, 19, 26 i 46 px). Zgrubny
szacunek przy 4 bitach na piksel i zestawie łacińskim z polskimi znakami: rozmiary tekstowe po
kilka do kilkunastu kilobajtów, 26 px około dwudziestu kilku. Rozmiar 46 px służy wyłącznie do
odczytu głośności, więc wystarczy w nim **podzbiór z samymi cyframi** — kilka kilobajtów zamiast
kilkudziesięciu. Razem rząd kilkudziesięciu kilobajtów przy 16 MB pamięci, czyli bez znaczenia.

Licencja Ubuntu Font Licence pozwala osadzać krój w urządzeniu. Przy bring-upie warto sprawdzić, czy
sześć rozmiarów jest naprawdę potrzebnych — zejście do czterech to najprostsza oszczędność, jaka tu
jest do wzięcia.

## Jak dodać ekran

1. Dopisz wpis do `SCREENS` w `index.html`: nazwa, pomieszczenie, ikona, opcjonalnie klucz zasilania
   i identyfikator urządzenia dla paska niedostępności.
2. Dodaj `<section class="scr" data-scr="…">` w `.content`.
3. Złóż treść wyłącznie z komponentów z `system.html`.
4. Dodaj zakładkę albo wiersz w `WIĘCEJ`.
5. Nie dopisuj kolorów ani wymiarów — jeśli czegoś brakuje, najpierw dopisz to do tokenów.

## Różnice wobec zatwierdzonej specyfikacji

Przyjęcie tego kierunku wymaga ADR, bo narusza cztery ustalenia:

- **Zakładki.** Spec wymaga dwóch równych zakładek `TV | GŁOŚNIKI`. Tu jest pięć.
- **Apple TV i biblioteka.** Spec ich nie przewiduje. Dochodzą nowe polecenia MQTT (nawigacja,
  uruchomienie aplikacji, odtworzenie pozycji), nowy temat na metadane biblioteki oraz pobieranie
  obrazów po HTTP. Każde z tego wymaga ADR-u i aktualizacji kontraktu.
- **Układ Z906.** Spec zatwierdza „Soundstage” z dekoracją orbitalną i trzema kolorami kanałów. Tu
  jest ta sama zawartość (pierścień, źródła, trzy kanały, efekt, `Decode 5.1`) w jednolitym stylu,
  bez dekoracji i bez różnicowania kolorem.
- **Nagłówek na ekranie TV.** Spec przewiduje tam oba włączniki (TV i Z906). Tu zasilanie Z906 jest
  wyłącznie na ekranie dźwięku.

Bez zmiany pozostają: kontrakt MQTT, model danych, zasada świeżego stanu po wybudzeniu, zakresy
0–100 z krokiem 2, `Decode 5.1` jako wskaźnik oraz zakaz zgadywania encji LG.

## Dane

Logotypy Infuse i YouTube pochodzą z biblioteki Simple Icons. `assets/wppilot.svg` to biały napis
z nagłówka pilot.wp.pl (symbol `#logo`, pobrany 2026-11-03) — na kafelku stoi jak napisy TVP.

**Plakaty makieta naprawdę pobiera po HTTP**, z serwisu losowych zdjęć `picsum.photos`, w docelowym
rozmiarze 200 × 300. To nie są okładki tych tytułów, tylko przypadkowe fotografie — chodzi o to, by
ocenić wygląd karuzeli i zarazem przećwiczyć prawdziwą ścieżkę: szkielet w trakcie pobierania,
podmianę po odebraniu obrazu i zapas lokalny przy błędzie. Jeden plakat waży stamtąd ok. 8 KB, czyli
dokładnie tyle, ile zakładam dla Emby. Pliki w `assets/posters/` to tylko te zapasowe tła.

Makieta pracuje na danych symulowanych. Wartości źródeł, trybów obrazu i efektów odpowiadają listom
z [`../../architecture/mqtt-contract.md`](../../architecture/mqtt-contract.md) i
[`../../integrations/z906.md`](../../integrations/z906.md), ale nie są pobierane z Home Assistanta.
Skróty kanałowe i ekran `DOM` są wizualne — nie mają jeszcze odpowiedników w kontrakcie.
