# ADR 0002: Home Assistant jest granicą urządzeń

[English](0002-home-assistant-boundary.en.md)

- **Status:** zaakceptowane
- **Data:** 2026-10-29

## Decyzja

Panel komunikuje się z Home Assistant przez dedykowany kontrakt MQTT. Home Assistant wykonuje komendy względem TV, Z906 i pozostałych urządzeń oraz zwraca pełny snapshot ich stanu.

## Uzasadnienie

Z906 jest już obsługiwany przez osobny sterownik ESP32 i Home Assistant. Powielenie jego protokołu w pilocie tworzyłoby drugi punkt awarii. Centralna granica upraszcza UI, automatyzacje i przyszłą wymianę urządzeń.

## Konsekwencje

- Pilot nie zawiera IR ani bezpośredniego UART dla Z906.
- Pilot nie zależy od identyfikatorów encji Home Assistant.
- Ewentualne bezpośrednie połączenie z webOS może powstać później tylko jako osobny adapter oraz ADR, bez zmiany API UI.
