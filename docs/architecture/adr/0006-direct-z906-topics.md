# ADR 0006 — Pilot rozmawia z Z906 wprost po MQTT

- **Data:** 2026-10-30
- **Status:** przyjęta
- **Zmienia:** [ADR 0002](0002-home-assistant-boundary.md) w zakresie dźwięku

## Problem

ADR 0002 ustalił, że panel jest klientem Home Assistanta: publikuje wyłącznie wersjonowany
kontrakt `multimedia_remote/v1`, a tłumaczeniem na encje zajmuje się bridge po stronie HA.
Taki bridge powstał i działa.

Droga polecenia wygląda w nim tak:

```text
pilot → broker → Home Assistant → broker → kontroler Z906
```

Przy tarczy głośności, gdzie palec oczekuje natychmiastowej reakcji, przejście przez Home
Assistanta jest słyszalne. Każdy przeskok to kolejne kolejkowanie i kolejna automatyzacja
do wykonania.

## Decyzja

Dla **dźwięku** pilot rozmawia bezpośrednio z tematami kontrolera Z906:

```text
pilot → broker → kontroler Z906
```

Kontroler ma własne, stabilne tematy i publikuje pełny stan. Pilot subskrybuje je i publikuje
polecenia na jego temat `command`. Home Assistant nie przestaje działać — nadal widzi ten sam
kontroler i te same tematy, więc jego automatyzacje i encje zostają bez zmian.

## Co to kosztuje

**Pilot zna wewnętrzny protokół innego urządzenia.** Zmiana kontrolera albo jego tematów
wymaga zmiany firmware'u. Dotąd taka zmiana kończyłaby się na automatyzacji w HA.

**Dwa urządzenia sterują tym samym sprzętem niezależnie.** Kontroler publikuje stan po każdej
zmianie, więc pilot dowie się o cudzym poleceniu — ale rozstrzyganie kolizji przestaje mieć
jedno miejsce.

**TV, sceny i skróty zostają bez źródła.** Nie mają własnego kontrolera MQTT, więc dla nich
droga przez Home Assistanta pozostaje jedyną sensowną — ale dopóki nikt jej nie pisze, ich
ekrany pokazują „brak danych". Pilot będzie docelowo mówił dwoma językami naraz i trzeba to
utrzymać w jednym interfejsie.

## Co zostaje bez zmian

- Kontrakt `multimedia_remote/v1` i bridge w [`../../../homeassistant/`](../../../homeassistant/)
  zostają opisane i zainstalowane po stronie HA — to tam wróci TV, gdy przyjdzie jego kolej.
  Z firmware'u klient kontraktu znika: nie ma sensu trzymać drugiego połączenia, które dziś
  nie obsługuje żadnego ekranu. Wróci razem z TV.
- Zasada świeżego stanu z [ADR 0005](0005-fresh-state-after-wake.md): dotknięcie nie przesuwa
  zaznaczenia, robi to dopiero stan odebrany z kontrolera.
- Zakresy 0–100 z krokiem 2 oraz `Decode 5.1` jako wskaźnik.
- Dane dostępowe i identyfikator kontrolera żyją w NVS urządzenia, nie w repozytorium.

## Tematy kontrolera

Identyfikator ma postać `z906_<mac>` i wpisuje się go na urządzeniu. Stan przychodzi
w osobnych tematach, po jednej wartości w każdym, więc pilot składa z nich pełny obraz.

| Temat | Wartość |
| --- | --- |
| `z906/<id>/availability` | `online` / `offline` |
| `z906/<id>/state/power` | `ON` / `OFF` |
| `z906/<id>/state/mute` | `ON` / `OFF` |
| `z906/<id>/state/volume` | 0–100 |
| `z906/<id>/state/centre` | 0–100 — **pisownia brytyjska**, nie `center` |
| `z906/<id>/state/subwoofer` | 0–100 |
| `z906/<id>/state/rear` | 0–100 |
| `z906/<id>/state/source` | jedna z sześciu nazw wejść |
| `z906/<id>/state/effect` | jedna z czterech nazw efektów |
| `z906/<id>/state/decode` | `ON` / `OFF` |

Polecenia idą na `z906/<id>/command/<nazwa>` z tą samą wartością, której spodziewa się stan.
Nazwy wejść i efektów pokrywają się co do znaku z listami z
[`../../integrations/z906.md`](../../integrations/z906.md), więc nie ma czego tłumaczyć.

Kontroler nie podaje rewizji, więc numeruje ją pilot: każdy odebrany fragment stanu podnosi
licznik i daje nowy, pełny snapshot.
