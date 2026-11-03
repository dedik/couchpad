# ADR 0012 — Styl jasny i ciemny

[English](0012-light-and-dark-theme.en.md)

- **Data:** 2026-11-03
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0003](0003-single-ui-owner.md), [ADR 0010](0010-start-screen-after-wake.md)

## Problem

Pilot miał wyłącznie ciemny styl z makiety Pulpit. W dzień, w jasnym pokoju, ciemny ekran
wygląda jak wyłączony i odbija otoczenie; wieczorem jasny razi. Użytkownik chce wybrać styl
w ustawieniach: na stałe ciemny, na stałe jasny albo zależnie od godziny (np. 18:00–8:00
ciemny).

Ekrany są budowane raz i potem tylko wiązane ze stanem (zasada wydajności z
`ui-architecture.md`). Kolory trafiają do obiektów LVGL przy budowie — wspólnymi stylami
z `theme.cpp` i stylami lokalnymi ustawianymi w ekranach. Przebudowa całego interfejsu przy
zmianie stylu oznaczałaby usuwanie i odtwarzanie setek obiektów, wskaźników w `Context`
i podstron budowanych przy pierwszym otwarciu.

## Decyzja

**Dwie palety o tych samych rolach i przemalowanie w locie, bez przebudowy ekranów.**

- `theme::Palette` ma 13 ról (`bg`, `surface`…`surface3`, `line`, `text`, `dim`, `faint`,
  `accent`, `accent_pressed`, `accent_ink`, `ok`, `danger`). `kDark` to dotychczasowe
  wartości z `pulpit.css`, `kLight` — jasny odpowiednik. `theme::bg()` i reszta zwracają kolor
  z bieżącej palety.
- `theme::apply(Mode)` dla każdej właściwości kolorowej (tło, gradient, obramowanie, obrys,
  cień, tekst, linia, łuk, przebarwienie obrazu) we wspólnych stylach i w stylach lokalnych
  wszystkich obiektów (`lv_layer_bottom`, ekran, `lv_layer_top`, `lv_layer_sys`) zamienia
  kolor roli ze starej palety na tę samą rolę z nowej, po czym woła
  `lv_obj_report_style_change(nullptr)`. Rolę rozpoznaje po wartości, dlatego **żadne dwie
  role jednej palety nie mogą mieć tego samego koloru**.
- Kolory nadawane przy rysowaniu (`render_*`) i podstrony budowane później biorą się
  z `palette()`, więc same są w bieżącym stylu.
- Kolor spoza palety nie zmienia się ze stylem. Jedyny celowy przypadek to
  `theme::logo_plate()` — ciemna płytka pod logotypami kanałów i aplikacji, bo część z nich
  (TVP, WP Pilot) jest biała. Jego wartość nie może się pokrywać z rolą żadnej palety.
- Ustawienie *Więcej → Ekran → Styl*: `ciemny` (domyślny, zachowanie sprzed zmiany),
  `jasny`, `wg godzin`. W trybie „wg godzin” drugi wybór daje okno stylu ciemnego: 17–7,
  **18–8** (domyślne), 19–7, 21–7. NVS: `theme`, `theme_hrs` (`Pref::ThemeMode`,
  `Pref::ThemeHours`).
- `effective_theme()` liczy styl z ustawień i godziny. `on_tick` sprawdza go co sekundę, także
  w uśpieniu, więc pilot budzi się już we właściwym stylu. Zanim pilot zna godzinę (brak
  synchronizacji czasu), „wg godzin” daje ciemny — tak jak startuje.
- Styl jest ustawiany przed `theme::init()`, więc przy stałym jasnym ekrany od razu powstają
  jasne, bez przemalowania przy starcie.

Rozważone i odrzucone:

- **Restart po zmianie stylu** — prosty, ale tryb „wg godzin” restartowałby pilota co wieczór
  i rano, a każda zmiana w ustawieniach kosztowałaby kilka sekund i ponowne łączenie.
- **Przebudowa interfejsu** — wymaga sprzątania `Context`, podstron i okien; dużo kodu dla
  rzadkiej operacji i realne ryzyko wycieków pamięci wewnętrznej.
- **Domyślny motyw LVGL** (`lv_theme_default`) — projekt go nie używa (`CONFIG_LV_USE_THEME_DEFAULT`
  wyłączone); wszystkie obiekty mają własne style z `theme.cpp`.

## Skutki

- Ekrany nie wiedzą o stylach. Zasada „ekrany nie definiują własnych kolorów” staje się
  twarda: kolor wpisany w ekran nie zmieni się ze stylem.
- Nowy kolor to nowa rola w obu paletach (i w `pulpit.css` makiety, pod
  `[data-theme="light"]`).
- Przemalowanie przechodzi raz po całym drzewie obiektów — kilka milisekund, raz na zmianę.
- Ikony pixel art i obrazy pogody zostają bez zmian; na jasnym tle sprawdzone na urządzeniu.
- Zrzuty dokumentacji są w obu stylach: `docs/screenshots/` (ciemny) i
  `docs/screenshots/light/`.
