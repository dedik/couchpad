# ADR 0020 — Remote tab

[Polski](0020-remote-tab.md)

- **Date:** 2026-11-10
- **Status:** accepted
- **Extends:** [ADR 0010](0010-start-screen-after-wake.en.md), [ADR 0013](0013-landscape-orientation.en.md), [ADR 0016](0016-android-app.en.md)

## Problem

The device is first of all a remote for the TV, Apple TV and speakers, yet everyday control was
spread over three screens. Watching a film via Apple TV meant switching between the trackpad
(Apple TV), volume (Z906) and input or P± (TV).

## Decision

- **A new first screen, "Pilot"** (`ScreenId::Remote`, `components/ui/screens/screen_remote.cpp`;
  in the app `ui/remote/RemoteScreen.kt`) with three blocks and only the essential controls:
  - **TV:** inputs as segments with an icon — live TV and inputs named in the TV settings;
    inputs with a default name ("HDMI 2") stay on the full TV screen unless active; **the active
    one always comes first** (`domain/tv_sources.hpp`); picture mode in one line ‹ name › (arrows
    — previous / next, wrapping; name — full list; dots — position); Back, P−, P+. In landscape
    also channel shortcuts and the channel list.
  - **Apple TV:** the same trackpad as the Apple TV screen, Menu, Home (tap, ×2, hold) and
    Keyboard; power in the block header. The keyboard also opens by itself on this screen when
    Apple TV shows a text field.
  - **Z906 volume:** −/+ by one amplifier level (hold repeats), value, mute. In portrait the audio
    input sits next to the speaker icon. In **landscape** (150 px column, mockup
    `docs/ux/mockup-remote-volume.html`, G2) a header like TV and Apple TV (icon, "Z906",
    Decode 5.1), the input as a tile with the current value (tap — the list from the Speakers
    screen), the four effects shown explicitly 2 × 2 (3D, 2.1, 4.1, None) and −/+ side by side.
  No app logos, Z906 channel levels, effects or Decode — they stay on the full screens.
- **No shell header** on this screen. In **landscape** no side tab rail either: it hides in a
  drawer at the left edge, as deeper in the Home tab (28 px grab strip, 5 × 64 px handle, swipe
  ≥ 36 px or tap). The 88 px gained go to 68 px keys (`kTouch`) and channel shortcuts (mockup
  `docs/ux/mockup-remote-panel.html`, variant L3). In **portrait** the TV and volume blocks have no
  header (variant P2): the input segments and the speaker icon speak for themselves, and the space
  goes to 68 px keys.
- **The tab bar has six items**, Pilot first. In landscape a tab is 72 px tall
  (6 × 72 + 5 × 8 = 472 ≤ 480; was 80), in portrait about 71 px wide (was 86) — still ≥ the
  68 px touch target.
- **Start screen:** a round Pilot button at the crossing of the tiles (132 px, accent outline,
  72 px icon, a 12 px ring in the background colour cuts an arc into the tile corners). The top
  row of tiles shows name and state at the top so the button covers nothing. The left / top part
  of Start is unchanged.
- **MQTT contract unchanged.** Pilot sends the same intents as the full screens (`tv_*`,
  `appletv_*`, `z906_step_volume`, `SetMuted`).
- The tab was built in the app first (a departure from "the panel is the reference"); this ADR
  brings it to the panel, so both versions match again (with different sizes — the panel has
  ≈ 8.6 px per mm, so its keys are larger than in the app).

## Consequences

- `tools/remote.py`: new portrait tab coordinates and a `15-remote` shot in `tour`.
- New pixel-art icons: `tools/draw_remote_icons.py` (`remote`, `key_*`), in the firmware via
  `tools/gen_assets.py` (`icon_remote`, `icon_remote_hub` 72 px, `icon_key_*` 24 px,
  `icon_tv_small`, `icon_appletv_small` 24 px).
- The TV screen remains the full view (d-pad, VOL, TV menu); Pilot does not replace full screens.
