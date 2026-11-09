# ADR 0018 — Radiators: thermostatic heads in the rooms

[Polski](0018-radiators.md)

- **Date:** 2026-11-09
- **Status:** accepted
- **Extends:** [ADR 0009](0009-climate-via-home-assistant.en.md), [ADR 0014](0014-rooms-and-lights.en.md), [ADR 0017](0017-room-climate-card.en.md)

## Problem

The living room, bedroom and bathroom have SONOFF TRVZB thermostatic radiator heads
(Zigbee2MQTT, `climate.*` entities), and a fourth one is being added to the office. The remote
did not show them: a room listed only its light and the air conditioner. The kitchen and the
hallway have no radiator and never will.

The head measures temperature next to the radiator, so its `current_temperature` reads high
(living room: head 24.5°, room sensor 22.6°). It also keeps its own weekly schedule
(`text.*_weekly_schedule_<day>`, six `HH:MM/degrees` points per day), used in `auto` mode.

## Decision

- **A "Grzejnik" (radiator) card in the room and a device screen**, following the Home tab
  standard: a setpoint dial (rule 3), power in the header, tiles beside it. Mockups:
  `docs/ux/mockup-dom/rooms.js`.
- **Room temperature only from the room sensor** — the same one as the "W pokoju" card
  (ADR 0017, `indoor.rooms` in the weather message). The radiator snapshot deliberately has no
  head temperature field. A missing sensor reading hides the marker and the "w pokoju" caption;
  the head's value is never substituted.
- **Modes:** `heat` — "Grzanie" (fixed setpoint), `auto` — "Harmonogram" (the head's own
  schedule). Off is the header power button (`climate.turn_off`); on returns to the last mode
  (`climate.turn_on`), as with the air conditioner.
- **The schedule is view-only.** The bridge computes the next schedule point that changes the
  temperature (`next: {at, target}`) and the remote shows it in Harmonogram mode. Editing stays
  in Home Assistant / Zigbee2MQTT — six points for seven days is too much for the remote.
- **Dial:** range from the entity clamped to 5–30 °C (the entity allows 4–35), step from the
  entity (0.5). The arc goes from blue (cold) through amber to red (hot) — a ring image used as
  the arc texture. A white dot on the arc is the room sensor temperature. One command per
  gesture, as with the air conditioner.
- **Tiles:** Mode (Grzanie, Harmonogram), Now (valve: heating / not heating from
  `hvac_action`; window; head battery; the next point in Harmonogram, target – room difference
  in Grzanie), Quick (17°, 19°, 21°, 23° — set the setpoint).
- **Window from the room's contact sensor, not the head.** The TRVZB does not report an open
  window: `switch.*_open_window` enables the detection feature, it is not a reading. The bridge
  takes the state from the window contact sensor assigned to the room (`window` in
  `radiator_rooms`, a `binary_sensor`). A room without a sensor has no "Okno" tile. With the
  window open the card says "okno otwarte" instead of "grzeje".
- **Contract:** a `radiators` snapshot section and `radiator.power.set`, `radiator.mode.set`,
  `radiator.target.set` actions keyed by the radiator id. Additive change, the version stays
  `v1`. Only the bridge knows the entities (`radiator_rooms` table).
- **Pixel art icons:** `trv_auto` (calendar with clock), `mode_heat` (same HA id as the air
  conditioner's mode), valve `trv_valve_on/off`, window `trv_window_open/closed`, battery `trv_battery_ok/low`, `trv_next`,
  `trv_delta`; card well `dev_radiator` (heating) and `dev_radiator_idle`.

## Consequences

- The snapshot grows by about 220 bytes per radiator. Firmware reads at most four.
- A new head is an entry in `radiator_rooms` plus its entities in the "snapshot after radiator
  change" automation triggers, no firmware change (as long as the room is one of the six).
- The heads currently have `temperature_sensor_select = internal`, so the room sensor value the
  "czujnik temperatury do termostatu" automations write to `number.*_external_temperature_input`
  does not drive the valve. That is head configuration, outside this decision — the remote
  shows the room sensor either way.
