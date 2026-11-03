# Board bring-up

[Polski](bring-up.md)

## Goal

Before implementing business features, the operation of exactly this board must be verified: display, touch, ESP32-C6 and Wi-Fi. Only the result of this procedure allows pinning the versions of ESP-IDF and the components.

## Safety rules

- Do not erase the flash and do not flash firmware without the user's consent.
- Do not accept pins or timings from other 4.3-inch displays as fact.
- Record all confirmed values in `docs/hardware/verified-board-map.md` with the date, SDK version and test method.

## Acceptance criteria

1. The device runs a minimal image over the full 480 × 800 area.
2. Touch reports correct coordinates in every corner and after the screen is rotated.
3. The ESP32-C6 connects to Wi-Fi, maintains the connection and enables MQTT.
4. A sample LVGL screen responds to touch without resets and without visible artefacts.
5. The boot log, the commit of the example sources and the dependency versions are recorded in the validation report.

After a successful test we will extract the hardware code into `components/board/` and forbid the remaining modules from using pins directly.
