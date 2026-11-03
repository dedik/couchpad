# Telewizor LG OLED77C11LB przez Glasshouse

Stan na 2026-11-02, Glasshouse 0.138.0, webOS (C1, 2021). Decyzja: [ADR 0008](../architecture/adr/0008-tv-via-glasshouse.md).

## Konfiguracja na pilocie

Ekran `WIĘCEJ → MQTT`, wiersz „Telewizor”: prefiks tematów z zakładki MQTT w panelu
Glasshouse, domyślnie `lgtv` (ustawia się sam). Pusty wyłącza telewizor.

Broker, użytkownik i hasło są wspólne z Z906 i Apple TV. Broker nie ma ACL, więc konto pilota
widzi tematy `lgtv/...` bez zmian. Konto `lgtv` w brokerze należy do telewizora.

Skróty kanałów potrzebują po stronie Home Assistanta trzech wpisów w `secrets.yaml`
(patrz [`homeassistant/README.md`](../../homeassistant/README.md)).

## Co działa

| Na pilocie | Polecenie | Potwierdzenie |
| --- | --- | --- |
| D-pad, OK | `command/rcu` `up/down/left/right/ok` | brak (chwilowe) |
| Cofnij | `command/rcu` `back` | brak |
| Menu TV | `command/rcu` `settings` (szybkie ustawienia LG) | brak |
| Źródło | `command/input` `livetv` / `hdmi1`–`hdmi4` | `state/application/app` |
| Tryb obrazu | `command/picture_mode` `<id>` | `state/picture/pictureMode` |
| Polsat, TVN, TVP 1, TVP 2 | kontrakt v1 `tv.channel.open` → HA → HTTP `POST /api/home/open` | brak (chwilowe) |
| Menu kanałów (ostatni kafelek) | kontrakt v1 `tv.channel.open` z `id` z listy `channels` → HA → `open` | brak (chwilowe) |
| P + / P − | `command/rcu` `channelup` / `channeldown` (wymaga łatki Glasshouse, ADR 0008) | brak (chwilowe) |
| VOL + / VOL − | głośność Z906 (`z906/<id>/command/volume`), krok 2 | stan Z906 |

Źródła na tym telewizorze: `TV` (Live TV), `HDMI 1`, `HDMI 2`, `PC` (HDMI 3), `Apple TV` (HDMI 4).
Nazwy HDMI pochodzą z ustawień TV i przychodzą w telemetrii, więc zmiana nazwy na TV zmienia
podpis na pilocie.

Tryb obrazu otwiera listę na podstronie, bo trybów bywa osiem, a nazwy w stylu „Dolby Vision
Cinema Bright” nie mieszczą się w rzędzie segmentów.

## Kanały

Sprawdzone 2026-11-02: lista tunera ma 55 kanałów DVB-T. Wszystkie cztery skróty znajdują
swój kanał po nazwie: `TVP1` (1), `TVP2` (2), `Polsat` (4), `TVN` (5) — także szablonem
bridge'a, uruchomionym w silniku szablonów HA na prawdziwej liście. Przełączenie z TVN na
Polsat (`POST /api/home/open` z komputera) trwało 0,8 s, `GET /api/home/now` potwierdził kanał.

## Czego nie ma

- **Zasilanie.** Wake-on-LAN na TV jest wyłączony; przycisk zostaje bez działania.
- **Głośność.** Dźwięk idzie optykiem do Z906, encje głośności TV są `unavailable`.
- **Odtwarzanie, klawiatura, aplikacje.** Glasshouse je ma (`command/playback`,
  `command/keyboardText`, `command/launch_app`), ekran TV jeszcze nie.
