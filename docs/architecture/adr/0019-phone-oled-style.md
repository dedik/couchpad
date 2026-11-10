# ADR 0019 — Styl OLED w aplikacji na Androida

[English](0019-phone-oled-style.en.md)

- **Data:** 2026-11-10
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0012](0012-light-and-dark-theme.md), [ADR 0016](0016-android-app.md)

## Problem

Ciemny styl pilota powstał pod ekran IPS płytki: czarne tło, karty w szarościach `#15171C`
i jaśniejszych, akcent pomarańczowy wypełniający duże pola, tekst niemal biały. Aplikacja na
Androida (ADR 0016) odwzorowuje go 1:1, ale działa też na telefonach z ekranem OLED/AMOLED, gdzie:

- duże szare karty zajmują większość ekranu, więc zalety czerni (kontrast, mniej prądu) giną;
- szarości tuż nad czernią każdy panel oddaje inaczej — przy niskiej jasności zlewają się
  z czernią albo barwią na fioletowo, przy przewijaniu smużą;
- pełny pomarańcz na dużych polach (OK, szerokie przyciski) i niemal biały tekst rażą
  w ciemnym pokoju (poświata jasnych liter na czerni);
- najsłabsze podpisy (`faint #626A78`, kontrast 3,9:1) są słabo czytelne.

Android nie mówi wiarygodnie, czy ekran to OLED, więc wyboru nie da się zrobić automatycznie.

## Decyzja

- **Osobny wariant ciemnego stylu, tylko w aplikacji:** *Ustawienia → Telefon → Styl ciemny
  na tym telefonie: standard / OLED*. Domyślnie standard. Preferencja `oled`
  (`Preferences.oled`, `shared_prefs/pilot.xml`) — lokalna dla telefonu, nie trafia do MQTT
  ani firmware'u. Działa, gdy obowiązuje styl ciemny (ciemny albo wg godzin w ciemnych
  godzinach); styl jasny zostaje bez zmian.
- **Paleta `OledPalette`** (`phone/native/.../ui/theme/Theme.kt`):
  - tło, karty i kontrolki w środku czysto czarne (`surface`, `surface2` = `#000000`);
    kształt rysuje wyłącznie obrys `line #2E3137`;
  - elementy, które w zwykłym stylu odróżnia od tła tylko wypełnienie (kafle efektów,
    trybów klimy i grzejnika, karty kanałów Z906, ikona w nagłówku), dostają obrys
    `C.edge` — w zwykłych stylach przezroczysty;
  - szarość zostaje tylko na wciśnięciu (`C.pressed` = `surface3 #24272B`) i na torach łuków;
  - tekst ok. 75% bieli (`#C2C5CA`), `dim #8C929B`, `faint #777D87` (kontrast ≥ 4,5:1);
    gałka na łuku w kolorze tekstu zamiast czystej bieli; szarości neutralne, bez
    niebieskawego odcienia;
  - akcent `#F57C14` (zapas na nasycające tryby wyświetlania, np. Vivid); duże pola akcentu
    — OK w TV i szerokie przyciski — jako miękkie tło 16% z obrysem i napisem w akcencie
    (`Palette.softAccent`); zasilanie i łuki zostają pełne (małe elementy);
  - płytka pod logotypami kanałów czarna jak karty (`Palette.logoPlate`).
- Flagi palety (`outlined`, `softAccent`, `logoPlate`) mają wartości domyślne równe dzisiejszym
  stylom, więc ciemny standardowy i jasny wyglądają jak dotąd.
- Przy okazji pasek nawigacji systemu idzie za kolorem tła bieżącego stylu (wcześniej zawsze
  ciemny, także w stylu jasnym).

## Skutki

- To świadome odstępstwo od zasady „aplikacja odwzorowuje panel 1:1” (AGENTS.md): dotyczy
  tylko ustawienia na stronie Telefon, która i tak istnieje wyłącznie w aplikacji. Panel ma
  ekran IPS i wariantu nie potrzebuje.
- Nowe miejsca rysujące karty lub kontrolki mają używać tokenów palety: `C.surface`/`C.surface2`
  na wypełnienie, `C.line` albo `C.edge` na obrys, `C.pressed` na wciśnięcie. Stała szarość
  wpisana na sztywno popsuje wariant OLED.
- Kolor tekstu nie zastąpi jasności ekranu: pilot narzuca jasność z *Ustawienia → Ekran →
  Jasność* (domyślnie 100%) niezależnie od systemowego suwaka. Na OLED-zie przy 100% nawet
  tekst 75% bieli świeci mocno — na telefonach OLED zalecana jest niższa jasność (40–60%)
  albo tryb nocny.
- Zrzuty w README pokazują styl standardowy; wariant OLED nie ma osobnego zestawu zrzutów.
