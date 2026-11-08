# ADR 0016 — Android app in `phone/`

[Polski](0016-android-app.md)

- **Date:** 2026-11-08
- **Status:** accepted
- **Supplements:** [ADR 0001](0001-native-stack.en.md), [ADR 0002](0002-home-assistant-boundary.en.md), [ADR 0005](0005-fresh-state-after-wake.en.md), [ADR 0010](0010-start-screen-after-wake.en.md)
- **Changes:** the scope of the "the interface is native: ESP-IDF + C++ + LVGL" rule in `AGENTS.md` — it applies to the panel

## Problem

The same remote is useful beyond the ESP32-P4 panel: on an old phone left on the couch or as a
second remote in another room. The first attempt ran the firmware's LVGL code on the phone through
SDL2 and an ESP-IDF/FreeRTOS compatibility layer. It worked, but the 480 × 800 image scaled up to
the phone's screen was blurry, and touch and the system (bars, lock screen, launcher) were hard to
handle.

The app was first developed in a separate repository next to this one. Its documentation and
screenshots drifted away from the firmware, and the public repository did not say a phone
version existed.

## Decision

**The remote has a second version: a native Android app (Kotlin + Jetpack Compose) in the
`phone/` directory of this repository.**

- `phone/native/` is the main app. `phone/android`, `phone/compat` and `phone/platform` are the
  earlier SDL/LVGL port — kept as a reference and a Mac simulator, not developed further.
- The app is **a second client of the same contract**: `multimedia_remote/v1`, the Z906 topics
  (ADR 0006) and Glasshouse (ADR 0008), the same Home Assistant package. It adds no topics of its
  own; a contract change goes through `mqtt-contract.md` and covers both versions.
- **The panel is the reference.** Screens, settings, texts and UX rules (`docs/ux/`, ADRs) are
  designed for the panel and the app mirrors them one to one, in portrait and landscape.
  Differences come only from the platform: Wi-Fi and screen brightness belong to the system, the
  app can replace the launcher and rotate with the phone.
- The panel's rules apply to the app where they make sense: fresh state after wake-up (ADR 0005),
  start screen after wake-up (ADR 0010), a touch never waits for the network, no secrets in the
  repository (broker details are entered on the phone).
- The "ESP-IDF + C++ + LVGL, no other GUI framework" rule applies to **the panel**. Kotlin and
  Compose are allowed only in `phone/`.
- The firmware does not depend on `phone/`. The SDL port uses the firmware's components (`..`)
  but changes nothing in them.

## Consequences

- One repository, one set of documentation: the main README describes both versions and the
  app's screenshots live in `phone/docs/screenshots/` (script `phone/tools/screenshots.sh`).
- A new screen or a UI change on the panel is also a task for the app, so the versions do not
  drift apart. The app's work status: `phone/docs/PROGRESS.md`.
- Building the app needs JDK 17 and the Android SDK, but neither is needed to build the firmware.
