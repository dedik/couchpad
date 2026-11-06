# ADR 0013 — Landscape orientation

[Polski](0013-landscape-orientation.md)

- **Date:** 2026-11-06
- **Status:** accepted
- **Complements:** [ADR 0003](0003-single-ui-owner.en.md), [ADR 0012](0012-light-and-dark-theme.en.md)
- **Changes:** the bottom-tabs requirement in `AGENTS.md` and the resolution in `docs/ux/tv-and-z906-ui.md`

## Problem

The interface was designed for portrait only, 480 × 800, the panel's native orientation.
The remote is also meant to sit on a table or in a stand, where landscape is more
comfortable: the keyboard gets wider keys, and screens with several zones (Z906, settings)
fit them side by side instead of stacked.

The ST7701 panel runs over DPI and cannot rotate the image itself (`swap_xy is not
supported`). The board has no gyroscope or accelerometer, so the remote does not know how it
is held.

## Decision

**Orientation is a user setting, changed only in Display → Orientation, and takes effect after
a restart.**

- `storage::Pref::Orientation`: 0 portrait, 1 landscape (90°), 2 landscape flipped (270°).
  `main` reads it before `board::start(rotation)`.
- `esp_lvgl_port` rotates (`sw_rotate`) every frame chunk on flush, on the CPU
  (`lv_draw_sw_rotate`). In portrait rotation is off: no second buffer, no cost. LVGL maps
  GT911 touch through the display rotation.
- Screens are built once (rule from `ui-architecture.md`), so changing orientation means a
  restart, not a live rebuild. `theme::landscape()` tells screens which layout to build.
- Every screen has two build layouts and **one** state binding (`render_*`). The landscape
  layout arranges the same widgets differently; no separate structs or duplicated data.
- Landscape follows the `docs/ux/mockup-landscape` mockup:
  - tabs in a vertical rail on the left (88 px) instead of the bottom bar;
  - TV: source and picture mode in one row in the header, channels plus Back and TV Menu on
    the left, d-pad with rockers on the right;
  - Apple TV: apps and three actions on the left, trackpad on the right (no now-playing card);
  - Z906: source and effect in the header next to the Decode pill, volume dial on the left,
    the three channels on the right with a 300 px track;
  - Home: climate card on the left with power, mode (picker) and −/+ (one command 0.7 s
    after the last tap), 2 × 2 scenes next to it;
  - Climate: dial on the left, modes as icon tiles (the selected one large on the left, the rest 2 × 2 next to it) and fan settings in a row on the right,
    with a gap below the power button;
  - Settings: list on the left, the selected sub-page docked next to it;
  - pickers as a 380 px sheet at the right edge over a dimmed screen;
  - keyboard with Cancel and Save in the text-field row.
- `tools/remote.py` receives the rotation in the screenshot header and rotates the image;
  remote touch takes logical coordinates.

## Rejected

- **Automatic rotation** — no orientation sensor.
- **Live UI rebuild** — deleting and recreating hundreds of objects and `Context` pointers; a
  restart is simpler and takes a few seconds.
- **PPA rotation** (`CONFIG_LVGL_PORT_ENABLE_PPA`) — the first version. Scrolling a list in
  landscape hung the LVGL task for good: it waited on the PPA while holding the lock, so the
  watchdog stayed quiet and `remote.py` screenshots got no frame. Reproduced 2/2, 0 hangs
  without PPA on the same gestures (2026-11-06). The PPA root cause is unknown; going back to
  it needs that found first.

## Consequences

- Every new screen or sub-page is designed in both orientations. Landscape content is
  672 × 383 px (after the tab rail and header), portrait 432 × 623 px.
- The bottom tabs are mandatory in portrait; in landscape the side rail with the same tabs
  plays that role.
- `tour` in `remote.py` only knows portrait coordinates.
- Landscape was checked with remote taps from `remote.py`; finger touch and 270° rotation still
  need confirmation on the board.
