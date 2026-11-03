# ADR 0007 — Apple TV przez Home Assistanta i kontrakt v1

- **Data:** 2026-10-31
- **Status:** przyjęta
- **Uzupełnia:** [ADR 0002](0002-home-assistant-boundary.md), [ADR 0006](0006-direct-z906-topics.md)

## Problem

Ekran Apple TV ma trackpad, przyciski Menu i Home, kafelki aplikacji oraz zasilanie
w nagłówku, ale nic za nimi nie rozmawia z urządzeniem. Apple TV nie ma własnego
kontrolera MQTT, jak Z906. Mówi protokołem Companion/AirPlay, który wymaga parowania HAP:
SRP, Ed25519, ChaCha20-Poly1305 i wykrywania przez mDNS.

## Decyzja

Apple TV obsługuje integracja `apple_tv` w Home Assistancie (pyatv). Pilot rozmawia z nią
kontraktem [`multimedia_remote/v1`](../mqtt-contract.md), a tłumaczeniem na encje zajmuje się
bridge w [`../../../homeassistant/`](../../../homeassistant/).

```text
pilot → broker → Home Assistant (apple_tv / pyatv) → Apple TV
```

Kontrakt dostaje sekcję `appletv` w snapshocie i akcje `appletv.*`. Zmiana jest addytywna:
stare klienty ignorują nieznaną sekcję, a bridge — nieznane akcje. Wersja w temacie zostaje `v1`.

Pilot mówi teraz dwoma językami naraz, jak przewidział ADR 0006:

- Z906 — wprost z kontrolerem (`z906/<id>/…`);
- Apple TV — przez kontrakt v1 (`multimedia_remote/v1/<remote_id>/…`).

Oba połączenia składa w jeden snapshot `LiveSource` (`components/application`). Rewizję
numeruje on sam, więc `AppController` i reguła „ignoruj nie nowszy snapshot” zostają bez zmian.

## Aplikacje identyfikuje bundle id

Nazwy aplikacji w `source_list` Apple TV bywają inne niż marketingowe i zależą od języka.
Kluczem kafelka jest więc bundle id (`app_id` w HA), a nazwa służy wyłącznie do wyświetlania.
Bundle id odczytuje się z prawdziwego urządzenia (patrz
[`../../integrations/appletv.md`](../../integrations/appletv.md)), nie zgaduje.

## Dlaczego nie natywnie na ESP32

- Parowanie HAP i szyfrowany kanał Companion to kilka tysięcy linii kryptografii, której
  sdkconfig dziś nie ma (ChaCha20, Poly1305, HKDF), oraz komponent mDNS.
- Apple zmienia te protokoły między wersjami tvOS; pyatv nadąża, firmware bez OTA nie.
- Klucze parowania byłyby kolejnym sekretem w NVS.

## Co to kosztuje

**Nawigacja idzie przez Home Assistanta**, czyli przez ten sam dodatkowy przeskok, który
w ADR 0006 był słyszalny przy tarczy głośności. Łagodzimy to tak:

- `appletv.navigate` jest zdarzeniem chwilowym: bridge nie publikuje po nim snapshotu;
- nawigację obsługuje osobna automatyzacja, która nie stoi w kolejce za Z906;
- pilot ogranicza przesunięcie po trackpadzie do kilku kroków na sekundę.

Jeśli to nie wystarczy, drogą zapasową jest mały mostek pyatv ↔ MQTT. Kontrakt zostaje
ten sam, więc zmienia się tylko strona serwera.

**Dwa połączenia MQTT po wybudzeniu.** Brak Home Assistanta nie blokuje dźwięku: nakładkę
łączenia zdejmuje snapshot Z906, a ekran Apple TV pokazuje „brak świeżego stanu”, dopóki
nie przyjdzie nowy snapshot z HA (ADR 0005).

**Klawiatura bez odczytu.** HA potrafi ustawić tekst pola na Apple TV, ale go nie zwraca.
Pilot wysyła zawsze pełny tekst (`appletv.keyboard.set`), a nie dopisuje znaków.

## Co zostaje bez zmian

- Z906 idzie wprost (ADR 0006).
- Sekcje „Nowe filmy” i „Nowe seriale” zostają lokalne; ich dane wymagają osobnego ADR-u.
- Identyfikator pilota (`remote_id`) i dane brokera żyją w NVS.
