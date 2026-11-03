# UI architecture

[Polski](ui-architecture.md)

## Design principle

The interface should look like a consumer product, not like an administrator's dashboard. The touch screen is the only primary input in v1, so frequent actions must be large, unambiguous and available without scrolling.

## Layers

```text
theme/       colours, typography, spacing, radii, active states
components/  icon+text button, device tile, slider, navigation button
screens/     Home, TV, Z906, Home Assistant, Settings, Diagnostics
binding/     AppState → properties of existing LVGL objects
```

The view sends only `UiIntent`. The application controller validates it, updates the pending state and passes the appropriate `Command` to MQTT. The displayed device state comes exclusively from `DeviceSnapshot`.

## Basic navigation

On the everyday control screens a discreet bottom bar with two tabs is always visible: `TV` and `GŁOŚNIKI` (Speakers). The selected tab indicates the current view and switches directly between the TV and Z906 screens — without returning to the Home screen and without any extra confirmation. The bar is a shared LVGL component, created once and updated together with the current screen.

## MVP screens

| Screen | Purpose | Key elements |
| --- | --- | --- |
| Home | instant control | TV/Z906 status, mute, volume, recent shortcuts |
| TV | everyday control | power, D-pad/OK, Home, Back, media |
| Z906 | audio | large volume, mute, source, effect, channel settings |
| HA | quick actions | deliberately chosen shortcuts, not a full dashboard |
| Settings | configuration | Wi-Fi, MQTT, brightness, version information |

The Z906 view uses the approved **Soundstage** layout: a central volume ring, a horizontal source selector, three vertical channel sliders, an effect selector and the decoding status. We do not replace it with a simplified status card.

## Performance rules

- The display runs in RGB565; graphics are pre-scaled and stored in a format suitable for LVGL.
- We do not rebuild the whole screen after receiving state. We update only the changed components.
- We do not create or delete LVGL objects on every refresh; screens are created once and bound to state.
- Animations are short, optional and never required to convey information.
- No I/O, no waiting for MQTT and no large object allocations in the touch callback.

The visual design is first produced as a 480 × 800 mockup, then reproduced in LVGL from the same tokens. This allows changing the style without disturbing the control logic.

The detailed, approved layout of the TV and Z906 screens is in [`../ux/tv-and-z906-ui.en.md`](../ux/tv-and-z906-ui.en.md). This specification is binding for the implementation of the LVGL components.

## Interface code map

A shortcut for the next person or AI: where things live and what pattern to follow when adding a screen,
so you don't have to read the whole of `components/ui` every time.

### State flow

1. Touch → the screen handler builds a `domain::UiIntent` (`domain/intent.hpp`) and calls
   `ui::dispatch()`. The handler never changes state itself.
2. `AppController::dispatch()` (`application/app_controller.cpp`) hands the intent to the source
   and marks the control as `Pending` (`AppState::mark_pending`: `requested` = `intent.text`,
   `requested_value` = `intent.value`, `requested_number` = `intent.number`).
3. The source (`SnapshotSource`) responds with a snapshot. Every snapshot with a higher revision
   replaces `AppState::snapshot()` and **clears all pending states**. No snapshot
   within `kConfirmTimeoutMs` yields `Failed`.
4. `ui::render()` (`ui.cpp`) redraws everything from state. It is the only path for data to the
   screen; `render_*` functions must be cheap and idempotent.

Source: `LiveSource`, which glues together
`Z906Source` (MQTT directly, ADR 0006), `HaClient` (Apple TV, air conditioning and channels,
v1 contract, ADR 0007/0009) and
`TvClient` (Glasshouse, ADR 0008). Intent routing: `targets_tv`, `targets_appletv`,
`targets_home_assistant`, `targets_climate` in `domain/intent.hpp`.

The snapshot (`domain/snapshot.hpp`) is about 3.6 kB. **Do not keep it on the stack** in code
called from the LVGL task (`pump()`, `poll()`) or in MQTT handlers (the `mqtt_task` stack
is 6 kB): events are class members (`AppController::event_`, `HaClient::outgoing_`),
and transport clients queue only the state of their own device (`Z906Event` carries
`Z906State`), not the whole snapshot. FreeRTOS queues live in internal memory.

### `components/ui` files

| File | Contents |
| --- | --- |
| `include/ui/theme.hpp`, `theme.cpp` | tokens from `pulpit.css`: colours, radii, spacing, shell dimensions, fonts, shared styles. Two palettes (dark, light) with the same roles; `theme::apply()` switches them at runtime by swapping role colours in the shared styles and in every object's local styles. Screens do not define their own colours — a colour outside the palette will not follow the theme. |
| `include/ui/widgets.hpp` | the whole component catalogue — start with this file |
| `widgets.cpp` | section, `make_fill`, `make_cap`, `Segmented`, `Tiles`, actions, `ListRow`, `Readout`, `Banner` + `set_unavailable`, power button, `Page`, `ask_confirm` |
| `widgets_controls.cpp` | continuous values: `Dial` (volume), `Thermostat` (temperature), `Level` (Z906 channels) |
| `widgets_nav.cpp` | D-pad, VOL/P rocker, Apple TV touchpad |
| `widgets_editor.cpp` | text editor with its own keyboard |
| `widgets_internal.hpp` | private widget helpers (`plain`, `attach`, `icon_label`) — not visible to screens |
| `ui_internal.hpp` | `Context` and screen structures (`TvScreen`, `ClimateScreen`…), `build_*`/`render_*` declarations |
| `shell.cpp` | shell: header, status bar, five tabs (`kScreens`), wake-up overlay |
| `ui.cpp` | `dispatch`, `render`, screen dimming and wake-up |
| `screens/screen_*.cpp` | one file per screen or subpage; `screen_start.cpp` is the start screen after boot and wake-up (ADR 0010) |

### Patterns a screen is built from

- **Tab screen:** `build_x(context, parent)` in `widgets::make_section`, `render_x(context)`
  added to `ui::render()`. No fresh state: `make_banner` + `set_unavailable(section, …)`.
- **Subpage** (anything deeper than a tab): `widgets::make_page(title, on_back, ctx)` —
  a full screen on `lv_layer_sys` with "Cofnij" (Back) in the header; `show_page(page, bool)`. The right side of the
  header (e.g. power) is added to `lv_obj_get_parent(page.title)`. Rarely used subpages
  are built on first opening (`ClimateScreen::built`) — free
  internal memory is about 180 kB.
- **Start screen** (ADR 0010) sits on `lv_layer_sys` above subpages; `show_start(true)`
  closes all subpages and dialogs (`close_overlays` in `screen_start.cpp`). **A new
  subpage must be added there**, otherwise after wake-up it will cover the device screen.
- **Picking from a long list:** a second `Page` with a `ListRow` for each option, without icon or arrow
  (`screen_tv.cpp` → `picker`, `screen_climate.cpp` → `picker`). The list closes immediately;
  the pending state is visible on the control that opened it.
- **Value in progress:** `state.pending_of(control)` gives what was requested — draw it instead of
  the snapshot value until the snapshot arrives (`render_half` in `screen_tv.cpp`,
  `shown_target` in `screen_climate.cpp`). A gesture widget (`Dial`, `Thermostat`) takes
  precedence while `busy()`.
- **Continuous values:** relative change, never a jump to the finger; one command after the finger is
  lifted (or 0.7 s after the last tap on −/+ on the `Thermostat`).
- **Callbacks:** captureless lambda + `Context*` as user data; list rows carry their own
  `{row, context, index}` structure.

### Memory

Internal memory (~768 kB, of which ~490 kB is free after boot) is a critical resource:
it holds task stacks, FreeRTOS queues and every allocation up to 4 kB
(`SPIRAM_MALLOC_ALWAYSINTERNAL`). PSRAM (32 MB) is almost empty. Rules:

- LVGL objects go to PSRAM through the allocator itself (`board/lv_mem_psram.c`) — nothing needs to be done;
- a large object for the whole runtime: `core::memory::make_in_psram<T>(...)`, not a global variable;
- data buffers (JSON, posters, lists): `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)`;
- after a change, check the `mem:` lines in the boot log (stages and stack headroom after 30 s);
- MQTT: a single connection (`transport::MqttBus`). A new client hooks in via `attach()`
  before `start()`, filters its own topics and sends via `bus.publish()` — never with its own
  `esp_mqtt_client_init` or `enqueue`;
- a task stack in PSRAM (`xTaskCreatePinnedToCoreWithCaps`) only when the task does not
  touch flash (NVS, OTA).

### Light and dark theme

Setting *More → Display → Style*: dark (default), light or "by hours" — dark inside the
chosen window (default 18–8), light outside it (`Pref::ThemeMode`, `Pref::ThemeHours`,
`effective_theme()` in `screen_device.cpp`). `on_tick` checks the theme once a second, also
while asleep. Until the remote knows the time, "by hours" stays dark. A new colour goes into
both palettes in `theme.cpp` (and into the mockup's `pulpit.css`); no two roles of one palette may
share a value. The only deliberate colour outside the palette is `theme::logo_plate()` — a dark
plate under logos, because TVP and WP Pilot are white. Decision and rejected options:
[ADR 0012](adr/0012-light-and-dark-theme.en.md).

### Fonts and icons

- Ubuntu 400/500/700 is generated by `tools/gen-fonts.sh` (bitmaps in `ui/assets/fonts`).
  Large readouts have a separate, small character set: `font_b46num`, `font_b64num` — digits,
  comma and the dash "—". Any other character in a large readout requires adding it to `num`
  in the script and running it again.
- Tab and start-tile icons: 48 × 48 pixel art from `docs/ux/mockup-pulpit/assets/pixel/`
  (`tools/draw_icons.py` → PNG → `tools/gen-assets.sh` → `I8` images, 48 and 96 px).
- Function icons are the built-in `LV_SYMBOL_*` (Montserrat). The mockup's set (lucide:
  snowflake, flame, fan…) is not available yet — which is why the air conditioning mode segments are
  text-only. Tab icons (48 px), start tiles (`icon_*_large`, 96 px) and logos are produced by
  `tools/gen-assets.sh`.

### Building

```bash
export PATH="/opt/homebrew/opt/python@3.13/libexec/bin:$PATH"
. ~/esp/esp-idf/export.sh && idf.py build
```

The ESP-IDF 5.5 environment is installed under Python 3.13; the default `python3` (3.15)
makes `export.sh` fail with an error about a missing virtual environment. Note: `export.sh … | tail`
returns exit code 0 even when `idf.py` does not exist — check the output, not the exit code.
