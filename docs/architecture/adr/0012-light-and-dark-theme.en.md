# ADR 0012 — Light and dark theme

[Polski](0012-light-and-dark-theme.md)

- **Date:** 2026-11-03
- **Status:** accepted
- **Supplements:** [ADR 0003](0003-single-ui-owner.en.md), [ADR 0010](0010-start-screen-after-wake.en.md)

## Problem

The remote only had the dark style from the Pulpit mockup. During the day, in a bright room, a dark
screen looks switched off and reflects the surroundings; in the evening a light one glares. The user
wants to choose the style in the settings: always dark, always light, or depending on the hour
(e.g. dark from 18:00 to 8:00).

Screens are built once and then only bound to state (performance rule in `ui-architecture.en.md`).
Colours reach LVGL objects at build time — through the shared styles in `theme.cpp` and through local
styles set by the screens. Rebuilding the whole interface on a style change would mean deleting and
recreating hundreds of objects, pointers in `Context` and sub-pages built on first open.

## Decision

**Two palettes with the same roles, repainted at runtime without rebuilding screens.**

- `theme::Palette` has 13 roles (`bg`, `surface`…`surface3`, `line`, `text`, `dim`, `faint`,
  `accent`, `accent_pressed`, `accent_ink`, `ok`, `danger`). `kDark` holds the existing values from
  `pulpit.css`, `kLight` their light counterpart. `theme::bg()` and the rest return the colour from the
  current palette.
- `theme::apply(Mode)` walks every colour property (background, gradient, border, outline, shadow,
  text, line, arc, image recolour) in the shared styles and in the local styles of every object
  (`lv_layer_bottom`, the screen, `lv_layer_top`, `lv_layer_sys`), swaps a role colour of the old
  palette for the same role in the new one, then calls `lv_obj_report_style_change(nullptr)`. It
  recognises the role by value, so **no two roles of one palette may share a colour**.
- Colours set while rendering (`render_*`) and sub-pages built later come from `palette()`, so they
  are already in the current style.
- A colour outside the palette does not follow the style. The only deliberate case is
  `theme::logo_plate()` — a dark plate under channel and app logos, because some of them (TVP,
  WP Pilot) are white. Its value must not match a role of either palette.
- Setting *More → Display → Style*: `dark` (default, the behaviour before this change), `light`,
  `by hours`. In "by hours" a second choice gives the dark window: 17–7, **18–8** (default), 19–7,
  21–7. NVS: `theme`, `theme_hrs` (`Pref::ThemeMode`, `Pref::ThemeHours`).
- `effective_theme()` derives the style from the settings and the hour. `on_tick` checks it once a
  second, also while asleep, so the remote wakes up already in the right style. Until the remote
  knows the time (no time sync yet), "by hours" gives dark — the style it starts with.
- The style is set before `theme::init()`, so with a fixed light style the screens are built light
  from the start, without a repaint.

Considered and rejected:

- **Restart after a style change** — simple, but "by hours" would restart the remote every evening
  and morning, and every change in the settings would cost a few seconds and a reconnect.
- **Rebuilding the interface** — requires tearing down `Context`, sub-pages and dialogs; a lot of
  code for a rare operation and a real risk of leaking internal memory.
- **LVGL default theme** (`lv_theme_default`) — the project does not use it
  (`CONFIG_LV_USE_THEME_DEFAULT` off); every object has its own styles from `theme.cpp`.

## Consequences

- Screens do not know about styles. The rule "screens do not define their own colours" becomes
  hard: a colour hard-coded in a screen will not follow the style.
- A new colour is a new role in both palettes (and in the mockup's `pulpit.css`, under
  `[data-theme="light"]`).
- A repaint walks the whole object tree once — a few milliseconds, once per change.
- Pixel-art icons and weather images stay unchanged; checked on the device against the light
  background.
- Documentation screenshots exist in both styles: `docs/screenshots/` (dark) and
  `docs/screenshots/light/`.
