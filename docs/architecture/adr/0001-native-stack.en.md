# ADR 0001: Native UI stack

[Polski](0001-native-stack.md)

- **Status:** accepted
- **Date:** 2026-10-29

## Decision

The firmware will use ESP-IDF, C++17 and LVGL 9. ESP-IDF is the native framework of the ESP32-P4; LVGL renders without a browser engine.

## Consequences

- We get low touch latency, full control over memory and MIPI-DSI, and a simple diagnostic path.
- The UI will require its own components and styling, but this yields a consistent, fast product.
- ESPHome, web UI, React, Flutter and similar layers are not part of the v1 remote firmware.

## Versions

The exact ESP-IDF tag and LVGL dependencies will be recorded after the bring-up procedure passes. The source of the base configuration is the manufacturer's example for the exact model, not an example for a similar board.
