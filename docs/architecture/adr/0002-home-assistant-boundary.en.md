# ADR 0002: Home Assistant is the device boundary

[Polski](0002-home-assistant-boundary.md)

- **Status:** accepted
- **Date:** 2026-10-29

## Decision

The panel communicates with Home Assistant through a dedicated MQTT contract. Home Assistant executes commands against the TV, the Z906 and other devices, and returns a full snapshot of their state.

## Rationale

The Z906 is already handled by a separate ESP32 controller and Home Assistant. Duplicating its protocol in the remote would create a second point of failure. A central boundary simplifies the UI, automations and future device replacement.

## Consequences

- The remote contains no IR and no direct UART for the Z906.
- The remote does not depend on Home Assistant entity identifiers.
- A possible direct webOS connection may be added later only as a separate adapter and ADR, without changing the UI API.
