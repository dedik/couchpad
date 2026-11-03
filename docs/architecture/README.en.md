# Firmware architecture

[Polski](README.md)

## Goal

The firmware is meant to be a fast touch remote for the TV, the Logitech Z906 and selected Home Assistant actions. The board is not yet another device controller: it is a responsive interface client, and Home Assistant remains the central place for automation and integrations.

The project is based on the Guition `JC4880P443C_I_W_Y`: ESP32-P4, ESP32-C6, a 4.3-inch 480 × 800 IPS display with an ST7701S driver, and capacitive touch. The manufacturer's official materials are the reference point for the bring-up example and the schematic: <https://github.com/guitionofficial/P4-series/tree/master/JC4880P443C_I_W/JC4880P443C_I_W>.

## Decisions in force

- **Framework:** ESP-IDF, C++17 and LVGL 9.
- **Application connectivity:** MQTT with the broker used by Home Assistant.
- **Integration boundary:** the panel publishes only the documented remote commands; Home Assistant executes the TV and Z906 services and publishes aggregated state.
- **Display:** portrait 480 × 800, RGB565, a simple native interface with no web layer.
- **Mechanics:** not part of this repository; they live in the separate `../enclosure` repository.
- **Wake-up:** the first touch starts the MQTT connection and a forced synchronisation with Home Assistant. The interactive TV/Z906 screen appears with a new, confirmed snapshot, targeting roughly 2–3 s.

We pin the ESP-IDF version after a successful test of the display, touch and ESP32-C6 on this board. We do not update it while implementing UI features without a separate hardware test.

## System map

```text
touch
  │
  ▼
LVGL UI ── UiIntent ──► application controller ──► MQTT ──► Home Assistant
  ▲                           │                                │
  │                           ▼                                ▼
  └──── render(AppState) ◄─ state store ◄─ state snapshot ◄─ TV/Z906 integrations

ESP32-P4: application, MIPI-DSI, LVGL, touch, MQTT
ESP32-C6: Wi-Fi/BLE as connectivity co-processor
```

## Wake-up and current device state

During a normal idle period the remote enters a power-saving mode. After a touch it may spend roughly 2–3 s waking up and bringing up Wi-Fi via the ESP32-C6 and MQTT. This time is meant for obtaining a **new** state from Home Assistant, not just restoring data from RAM.

Wake-up sequence:

1. A touch wakes the device and shows a lightweight "Łączenie z domem" (Connecting to home) screen.
2. The remote connects to Wi-Fi and the MQTT broker and publishes `remote.sync.request`.
3. Home Assistant fetches/merges the current states of the TV and Z906 integrations and publishes a full snapshot with a new revision.
4. Only after receiving it does the interactive Home, TV or Z906 screen appear.

If the connection or the response does not fit within the target time, the remote does not show old data as current. Instead it stays on the connecting screen with the option to retry. The specific low-power technique (light sleep or deep sleep) will be chosen after testing whether this board's touch can reliably wake the chosen mode.

## Code structure

```text
main/                       composition root; starts the modules
components/
  board/                    display, touch and C6 start-up verified only on this board
  core/                     event queues, clock, identifiers, basic types
  domain/                   AppState, UiIntent, Command, DeviceSnapshot
  application/              controllers and rules: intent → command / state → UI model
  transport/                Wi-Fi via ESP32-C6, eventually MQTT and v1 contract encoding
  storage/                  device settings in NVS, no secrets in sources
  ui/                       theme, components, screens, LVGL data binding
  diagnostics/              metrics, controlled logging and diagnostics screen
docs/                       contracts, ADRs, procedures and decision history
test/                       unit tests for hardware-independent logic
```

Dependencies flow only downwards: `ui → application → domain`, and `transport` and `storage` are adapters called by `application`. The UI does not import the MQTT or Wi-Fi client, and the domain modules know nothing about LVGL or ESP-IDF.

Adapters run in their own tasks and hand results back through queues, and `application` drains them from the UI task. This way a touch never waits for the network, and all LVGL calls stay in a single task (ADR 0003). The same pattern currently serves the simulated snapshot source and the Wi-Fi service.

The real snapshot source (`LiveSource`) combines three parts: the Z906 directly from its controller (ADR 0006), the Apple TV through the v1 contract via Home Assistant (ADR 0007), and the LG TV directly from the Glasshouse server on the TV (ADR 0008). From the outside it is one source with one revision, so `AppController` and the UI are unaware of the split.

## Implementation order

1. Verify the hardware and record the actual parameters in `docs/hardware/`.
2. Create a minimal `AppState`, an event queue and a static LVGL main screen.
3. Add the TV and Z906 screens on simulated data.
4. Implement the MQTT contract and connect it to Home Assistant.
5. Add Wi-Fi/MQTT configuration, diagnostics and OTA updates.

We do not combine the first stage with the battery, CAD, a camera, air mouse or the direct webOS protocol.
