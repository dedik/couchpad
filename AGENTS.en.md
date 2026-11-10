# Instructions for future AIs and people developing the project

[Polski](AGENTS.md)

## Before you start

1. Read `docs/architecture/README.md`, `docs/architecture/mqtt-contract.md`, `docs/ux/tv-and-z906-ui.md` and `docs/operations/ai-handoff.md`. When working on the Home tab, also `docs/ux/home-screen.en.md` (the standard: hierarchy, rules, components, adding devices).
2. Check `git status --short --branch` and do not delete other people's uncommitted changes.
3. Before modifying hardware code, read `docs/hardware/bring-up.md`.
4. The repository holds two versions of the remote: the panel firmware (the root) and the Android app in `phone/` ([ADR 0016](docs/architecture/adr/0016-android-app.en.md)). When working on the app, read `phone/README.en.md` and `phone/docs/PROGRESS.md`.

## Fixed v1 assumptions

- The target board is the Guition `JC4880P443C_I_W_Y`: the ESP32-P4 runs the application, the display and touch; the ESP32-C6 provides wireless connectivity.
- The panel does not control the Logitech Z906 amplifier directly, does not use IR and does not implement its UART protocol. It is a Home Assistant client over MQTT.
- The panel's interface is native: ESP-IDF + C++ + LVGL. Do not add a web server, HTML, JavaScript or an alternative GUI framework without an ADR. Kotlin and Jetpack Compose are allowed only in the Android app in `phone/` (ADR 0016).
- The default idle mode saves power. After a touch, the remote may spend up to about 3 s waking up, connecting to MQTT and forcing a synchronisation; the control screen shows only a new, confirmed snapshot of the TV and the Z906. Do not show remembered state as current.
- Do not store passwords, tokens, private network names or home infrastructure addresses in the repository, logs or examples.
- Do not flash firmware, erase the flash or change physical wiring without an explicit request from the user.

## Quality rules

- All LVGL calls are made exclusively by the UI task.
- A touch must not wait for the network; after an action the UI shows a pending state, and the state is confirmed by an MQTT message.
- When you change a module boundary, the MQTT format, the state model or a dependency, add an ADR and update the architecture documentation.
- Hardware code is written only after it has been confirmed on this specific board unit. Do not copy pins from similar Guition models.
- The approved Z906 screen is the Soundstage layout described in `docs/ux/tv-and-z906-ui.md`; do not replace it with a simplified card. The shared screen tabs are mandatory on the everyday control screens: at the bottom in portrait, in a side rail in landscape ([ADR 0013](docs/architecture/adr/0013-landscape-orientation.en.md)). Design every new screen in both orientations.

## Android app (`phone/`)

- The panel is the reference: design screens, settings and texts for the firmware and the app mirrors them one to one in portrait and landscape. Exception: the OLED style on the Phone page, app only ([ADR 0019](docs/architecture/adr/0019-phone-oled-style.en.md)) — draw cards and controls with palette tokens (`C.surface`, `C.line`/`C.edge`, `C.pressed`), never a hard-coded grey. A UI change on the panel is also a task for the app (or an entry in `phone/docs/PROGRESS.md`).
- The app speaks the same MQTT contract as the panel and adds no topics of its own.
- The firmware must not depend on `phone/`. The SDL port (`phone/android`, `compat/`, `platform/`) is a reference — do not develop it.
- Check a change visible on screen on a phone (`adb`) and after every such change refresh the screenshots in all four variants (portrait/landscape × dark/light): `phone/tools/screenshots.sh portrait|landscape [light]`. The script also rebuilds the README overviews `phone/docs/screenshots/overview-*.png` — a UI change commit must include the new screenshots so main always shows the current look. If the change moves elements, fix the tap coordinates in the script (a stale tap can hit a button that sends a command). Do not enter MQTT broker details for the user or store them in the repository.
