# ADR 0017 — Temperature and humidity on the room page

[Polski](0017-room-climate-card.md)

- **Date:** 2026-11-09
- **Status:** accepted
- **Supplements:** [ADR 0014](0014-rooms-and-lights.en.md), [ADR 0015](0015-indoor-average-on-start-screen.en.md)

## Problem

The room page showed only devices. Room sensor readings were visible only as the whole-home
average on the start screen (ADR 0015), although the rooms have temperature and humidity
sensors, and three of them also have thermostatic radiator valves.

## Decision

- **An "In the room" card above the devices.** Two readings side by side: a pixel-art icon
  (thermometer, droplet, `tools/draw_device_icons.py`, 24 grid at 2×), a large number and a
  coloured verdict below it: temperature 19–24.5 °C "comfortable", lower "cool", higher "warm";
  humidity 40–60 % "comfortable", 35–39 % "slightly dry", lower "dry", above 60 % "humid". No
  chart. In portrait the card sits under the room rail with the caption `W POKOJU`; in landscape
  it is the first, narrower column of the card row (temperature above humidity).
- **Source: the room sensor, not the valve.** The "Termostat" valves report no humidity and
  measure temperature next to the radiator, which would inflate the reading (see ADR 0015). The
  bridge uses the "<Room> - Czujnik temperatury" sensors, the same ones automations feed to the
  valves as external temperature. The kitchen is open to the living room and the hallway to the
  office, so they use those sensors (as in the ADR 0015 average). A room without a sensor has no card.
- **Rides with the weather.** An `indoor.rooms` field in the `weather` message: a list of
  `{room, temperature, humidity}`. It is content, not device state: retained, shown from memory,
  hidden after 6 hours like the weather (ADR 0011). The message is already sent on every change
  of these sensors. Additive change: an older remote ignores the field.
- **Air conditioner icon:** the AC card in the living room gets the air-conditioner icon
  (`dev_aircon`) instead of the cooling-mode icon.

## Consequences

- The bridge holds the per-room sensor list in the `multimedia_remote_publish_weather` script
  (`room_sensors`). A new sensor is a bridge change, not a firmware change.
- The weather message grows by about 250 bytes.
