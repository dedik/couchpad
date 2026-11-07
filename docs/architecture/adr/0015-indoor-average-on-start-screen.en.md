# ADR 0015 — Indoor average temperature on the start screen

[Polski](0015-indoor-average-on-start-screen.md)

- **Date:** 2026-11-07
- **Status:** accepted
- **Supplements:** [ADR 0011](0011-weather-on-start-screen.en.md)

## Problem

The left column of the landscape start screen had a lot of empty space under the weather.
Every room has a temperature and humidity sensor (Zigbee), yet the remote showed only the
outdoor weather.

## Decision

- **One average, not a list of rooms.** The start screen shows "at home": the average
  temperature and humidity of the room sensors. Room details belong to the Home tab.
- **The bridge computes it.** The sensor list lives in the `multimedia_remote_publish_weather`
  script (`indoor_temperature`, `indoor_humidity`). Unavailable sensors are skipped. NSPanels and
  thermostats are excluded — they heat themselves and inflate the average.
- **It travels with the weather.** The `weather` topic message gets an optional
  `indoor: {temperature, humidity}` field. It is content, not device state, so the weather
  rules apply (ADR 0011): retained, shown from memory, hidden after 6 hours. Additive change:
  a remote without support ignores it; a bridge without it makes the remote hide the tile.
- **Layout:** in landscape a 96 px clock and the date centred in the left column, below them
  "outside" and "at home" tiles; in portrait an "AT HOME 23.8° 39%" pill under the weather.
  Indoor temperature with tenths (half a degree matters indoors), outdoor to the whole degree.

## Consequences

- The weather message is also sent when any room sensor changes (a few times per quarter
  hour) — it is small and retained.
- Start-screen tile icons now sit in the middle of the free space above the name.
