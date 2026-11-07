# ADR 0014 — Rooms and lights on the Home screen

[Polski](0014-rooms-and-lights.md)

- **Date:** 2026-11-07
- **Status:** accepted
- **Supplements:** [ADR 0002](0002-home-assistant-boundary.en.md), [ADR 0009](0009-climate-via-home-assistant.en.md)

## Problem

The `DOM` screen had an air-conditioning tile and four scenes that the contract never
handled (momentary commands with no receiver). The user wants to control lights: for now
the living-room lamp — a Home Assistant `light` group of three WiZ bulbs (brightness,
colour temperature 2700–6500 K, 16 effects) — and eventually the main light of every room.
Mockups: `docs/ux/mockup-pulpit` (portrait), `docs/ux/mockup-landscape` (landscape), shared
logic in `docs/ux/mockup-dom/rooms.js`.

## Decision

**Screen.** Scenes are gone. Home is a grid of six rooms (living room, kitchen, hallway,
office, bathroom, bedroom) like the start screen, with a pixel-art room icon
(`tools/draw_room_icons.py`). A lamp in the tile corner shows the main light's state and
toggles it on tap. Air conditioning stays: a slim strip above the grid in portrait, a pill
in the header in landscape. Tapping a tile opens the room:

```text
Home → room (devices: light, A/C in the living room) → light (brightness, colour temperature,
       effect, bulbs) → single bulb or the effect list
```

Inside a room a room rail (the same icons, without lamps) switches rooms without going
back. In portrait it sits under the header. In landscape the room is full-screen and both
the tab rail and the room rail live in drawers: a swipe from the left edge (28 px strip,
≥ 36 px of travel) slides in the tabs, from the right edge — the rooms. Tapping the edge
strip opens them too (handles hint that they are there); tapping the dimmed background or
swiping back to the edge closes them.

**Contract.** Lights use the `multimedia_remote/v1` contract like the A/C: the snapshot gets
a `lights` section, commands get `light.*` actions. The change is additive; the version stays `v1`.

- **The remote does not know entities.** The bridge has a `light_rooms` table (room, id,
  label, entity, bulbs) and sends short ids (`lampa`, `lampa_lewa`). The bridge ignores
  commands with an unknown id.
- **The main light may be a switch** (`switch.*`): then `dimmable: false` and the remote
  shows on/off only.
- **Brightness in percent, colour temperature in kelvin.** The range and the effect list
  come from the entity. The remote offers seven evenly spaced colour steps and ten
  brightness steps.
- **An effect applies to the whole group.** An empty effect means plain white again: the
  bridge sets the current colour temperature, because WiZ leaves an effect only when the
  colour changes.
- **One snapshot per gesture.** A command changes the group and the bulbs at once, so the
  light-change automation waits 0.5 s (`mode: restart`). The bridge also publishes a
  snapshot after the command itself — otherwise a command that changes nothing would wait
  for confirmation until the timeout.

## Consequences

- A room without a `light_rooms` entry shows "no light". More rooms are an entry in the
  bridge table plus entities in the automation triggers — no firmware change (up to three
  rooms with a light and four bulbs per group).
- Without a `lights` section (a bridge from before this decision) tiles say "waiting for
  state"; the rest of the remote works as before.
- The HA client event grew by about 1.2 kB; its queue lives in PSRAM.
- The large A/C tile left the Home screen; the A/C subpage opens from the strip, the pill or
  the card in the living room.
- Room thermostat temperatures were only in the mockup and did not enter the contract.
