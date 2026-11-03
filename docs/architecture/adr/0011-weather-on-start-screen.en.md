# ADR 0011 — Weather on the start screen

[Polski](0011-weather-on-start-screen.md)

- **Date:** 2026-11-03
- **Status:** accepted
- **Supplements:** [ADR 0005](0005-fresh-state-after-wake.en.md), [ADR 0010](0010-start-screen-after-wake.en.md)

## Problem

The start screen shows a clock and four tiles. Next to the time there is room for the current weather,
and Home Assistant knows it (`weather.forecast_home`, met.no). The weather changes once every few tens of
minutes, so requesting it on every wake would waste traffic and memory.

## Decision

The bridge publishes the weather on a **separate retained topic**, like the channel list (ADR 0008), rather than
in the state snapshot:

```text
multimedia_remote/v1/<remote_id>/weather       Home Assistant → pilot, QoS 1, retained
{ "condition": "partlycloudy", "temperature": 5.5, "night": true }
```

It publishes it when the condition or temperature changes, at sunrise and sunset, and after
HA starts — not on request from the remote. `night` (sun below the horizon, `sun.sun`) is needed
because HA has a night variant only for a clear sky: `partlycloudy` at two in the morning is
the moon behind a cloud, not the sun.

Weather is **content, not device state**. ADR 0005 forbids showing device state from memory after wake,
but that does not apply to the weather: the remote keeps the last one in memory and shows it
immediately, without waiting for synchronisation. After a remote restart the broker hands back the last value by itself,
because the topic is retained. Weather older than 6 hours disappears from the screen instead of pretending to be current.

`condition` is an HA identifier (`sunny`, `rainy`…); the remote picks the Polish name and the icon.

## Consequences

- Zero extra requests on wake; one message per weather change.
- The state snapshot does not grow — the weather does not travel through queues or state copies.
- The entity is hard-coded in the bridge (`weather.forecast_home`).
