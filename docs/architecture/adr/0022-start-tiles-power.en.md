# ADR 0022 — Start screen: icon-only tiles, power and a status pill

[Polski](0022-start-tiles-power.md)

- **Date:** 2026-11-18
- **Status:** accepted
- **Changes:** [ADR 0010](0010-start-screen-after-wake.en.md) (start "controls nothing", one-line
  state per tile), [ADR 0020](0020-remote-tab.en.md) (name and state at the top of the upper row)

## Problem

The most common thing after picking up the remote is turning a device on or off. Today that means
entering the device screen and pressing power there. Meanwhile the start tiles repeated a name and
a one-line state that the icon and the device screen already tell.

## Decision

- **No captions on tiles.** Each tile is a large centred icon: 144 px in portrait (48 px drawing
  at 3 ×, `icon_*_xl`), 96 px in landscape (2 ×). The state line and the name leave the screen;
  the app keeps the name as a TalkBack description.
- **Power on the tile.** TV, Apple TV and Speakers get, in the top-left corner, the same power
  button as the device screen header (48 px, 64 px touch area; 40 dp `Solid` in the app). It sends
  the same intent (`SetPower` with `TvPower` / `AtvPower` / `ZPower`); the outline shows the
  pending state and a snapshot confirms it. Before a fresh state the button is dimmed and inactive
  (ADR 0005). Home has no button. Tapping the rest of the tile opens the device screen as before.
- **TV:** waking from standby needs Wake-on-LAN, which is disabled on the TV (ADR 0008), so the
  tile button — like the header — shows the state but changes nothing.
- **Status pill above the clock:** Wi-Fi strength (3 arcs, RSSI ≥ −60 / ≥ −70 dBm) and power.
  Panel: RSSI refreshed every 10 s in the Wi-Fi task only while start is visible; no cell yet, so
  "USB" (battery drawing waits for a measurement in `board`). App: phone battery and RSSI from
  local Android APIs. No new MQTT topics.
- Design: `docs/ux/mockup-tile-power`, `docs/ux/mockup-status` (variant B).

## Consequences

- The start screen controls devices for the first time — under the same rules as device screens
  (pending state, no control before a fresh snapshot).
- A tile no longer shows "no connection" for a single device; the "Connected to home" footer and
  dimmed buttons remain.
- Turning off is a single tap with no confirmation — easy to hit by accident. To be watched.
- Without captions nothing hides under the Remote circle, so the upper row no longer needs text
  at the top.
