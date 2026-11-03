# ADR 0003: A single task owns LVGL

[Polski](0003-single-ui-owner.md)

- **Status:** accepted
- **Date:** 2026-10-29

## Decision

All operations on `lv_obj_t` and all rendering are performed by a single UI task. The MQTT, Wi-Fi and storage tasks communicate with it through an event queue and do not touch LVGL objects.

## Rationale

LVGL does not provide a safe model for concurrent modification of views from multiple tasks. A single update path eliminates sporadic touch bugs and screen artefacts.

## Consequences

- The network cannot block interaction.
- Debugging is simpler: every view change originates from a `UiIntent` or a `DeviceSnapshot`.
