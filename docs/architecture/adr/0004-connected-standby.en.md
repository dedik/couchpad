# ADR 0004: Connected standby as the default idle mode

[Polski](0004-connected-standby.md)

- **Status:** superseded by ADR 0005
- **Date:** 2026-10-29

## Historical requirement

Touching the dimmed remote was supposed to immediately show the last confirmed state of the TV and the Z906.

## Historical decision

After an idle period we turn off only the screen and the backlight. The ESP32-P4, ESP32-C6, Wi-Fi, MQTT and the local `AppState` keep running. The remote subscribes to the full Home Assistant snapshot and receives its changes even while the screen is off.

On wake-up the UI renders the data already in RAM and, in parallel, sends `remote.sync.request`. A missing MQTT connection or a stale snapshot is explicitly signalled.

## Why it was superseded

The user accepted a delay of roughly 2–3 s after a touch, as long as the remote shows a new state. This should reduce battery power consumption and eliminate the risk of presenting the previous state as current.

- The response after a touch does not depend on the latency of the TV, Z906, Wi-Fi or Home Assistant.
- Idle power consumption will be higher than in deep sleep; we will measure it later before the final choice of battery parameters.
- Deep sleep may exist as a manual or critically-low-battery mode, but after it the first display of state requires reconnecting and must not pretend to be current.
