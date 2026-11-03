# UX specification — TV and Logitech Z906

[Polski](tv-and-z906-ui.md)

## Status

This is the approved v1 design specification for the native LVGL implementation. The source of truth for data remains the `DeviceSnapshot` from the versioned MQTT; sample values in the mockup are not the state of real devices.

## Shared navigation

- Target resolution: 480 × 800, portrait.
- At the bottom of both views there is a shared, always available bar with equal tabs `TV` and `GŁOŚNIKI` (SPEAKERS).
- The active tab is clearly highlighted. A tap switches directly between screens, with no Home screen and no confirmation.
- The bar touches the bottom edge of the screen's visual enclosure. There must be a discreet gap between it and the controls above; it must not look like it is part of them.
- All power buttons (TV and Z906 in the TV header, and Z906 on the speakers screen) use a single convention: on — aquamarine background and icon, off — neutral background and dimmed icon. Red is reserved for mute and errors and never indicates power state.
- Switching tabs does not read data from the network. It shows the current local `AppState`, which was previously confirmed by a snapshot.

## TV screen

### Composition

1. Header: `LG OLED`, location `SALON` (LIVING ROOM), a small TV power switch and a small Z906 power switch next to it.
2. A single row with two selectors side by side, without separate large cards labelled "ŹRÓDŁA" (SOURCES) and "TRYB OBRAZU" (PICTURE MODE):
   - on the left the current source, on the right the current picture mode, each as a single segment;
   - a tap opens a subpage with a list of options (the whole list row is the touch target).
3. A single row of five tiles: four channel shortcuts, logo-only (Polsat, TVN, TVP 1, TVP 2), and at the end a tile with a list icon that opens the "Kanały" (Channels) subpage: all tuner channels in channel-number order, each row with number and name, no arrows. No coloured backgrounds added to the TVP logos.
4. A centrally placed D-pad: Up, Down, Left, Right and `OK` exactly in the middle. On the sides, vertical rockers as on the LG remote: `VOL` on the left (**Z906** volume, step 2, with the limit from settings, repeats when held) and `P` on the right (program +/-, LG remote keys). Keep empty breathing space between the D-pad and the bottom buttons.
5. Two action buttons: `Cofnij` (Back) and `Ustawienia` (Settings).
6. The shared bottom tab bar.

### Data and behaviour

- TV data comes directly from the Glasshouse server on the TV ([ADR 0008](../architecture/adr/0008-tv-via-glasshouse.en.md), [integration](../integrations/tv.en.md)).
- Sources are dynamic: Glasshouse provides the list of inputs with their names from the TV settings, plus the currently selected one. Do not hard-code the list.
- Picture mode is dynamic and may change depending on the content. The list shows only the modes the TV will accept right now. Names like "Dolby Vision Cinema Bright" end with an ellipsis in the segment. Do not hard-code mode names.
- After a selection from the list, the segment shows the selected name with a pulsing outline until the TV confirms the change.
- Do not display extra purple chips or texts like `HDMI 1` / `Standard` outside the selection controls themselves — the selected state is already visible in the control.
- Channel shortcuts switch directly to the channel (Glasshouse tunes it and brings Live TV to the front by itself). The channel is selected by name from the tuner list, not by a hard-coded identifier.
- `Menu TV` opens the LG quick settings (the remote's settings key).
- Logos must be legible on a dark background and have a transparent background. Do not add your own blue or orange background under TVP 1/TVP 2.

## Logitech Z906 screen — Soundstage

This is the only accepted direction for the sound screen. Do not reduce it to a small card.

1. Header with only `Z906`, `SALON` and **one** Z906 power button. No TV header or second power switch may remain on the speakers screen.
2. Info row: current source and effect, e.g. `OPTICAL 1 · STEREO 2.1`.
3. A large central volume ring with a percentage number and a discreet orbital decoration. Mute is a small round button beside the ring.
4. Horizontal source selector: `PC`, `RCA`, `O1`, `O2`, `COX`, `AUX`.
5. "Scena dźwiękowa" (Soundstage) section: three vertical sliders `Center`, `Subwoofer`, `Tył` (Rear); the current value 0–100 is shown above the respective slider.
6. Effect selector: `Stereo 3D`, `2.1`, `4.1`, `Bez efektu` (No effect).
7. Info footer `Decode 5.1` and activity state. This is an indicator, not a button.
8. The shared bottom tab bar, separated by a small gap from the decode footer.

### Data binding

| UI element | Data from snapshot / bridge | Notes |
| --- | --- | --- |
| Power | `z906.power` | explicit `on` / `off`, no toggle guessing |
| Mute | `z906.muted` | small action button |
| Ring | `z906.volume_percent` | 0–100, sent at most 10/s plus a final value on release; controlled by relative rotation (below) |
| Source | `z906.source` + option list | real options from HA |
| Center/Subwoofer/Rear | corresponding HA numbers | 0–100, step 2; relative dragging (below) |
| Effect | `z906.effect` + option list | real options from HA |
| Decode 5.1 | binary state | read-only |

### Volume ring gesture

- Volume changes **relatively**: by the angle the finger rotates around the centre of the ring. A tap alone does not change the value — there is no jump to the touch point.
- Clockwise is louder, counter-clockwise quieter; a full turn corresponds to 100 points, the result is rounded to step 2 and clamped to 0–100.
- The centre of the ring (the area with the number) is a dead zone — the angle is unstable there, so movement there does not change the value.
- The `z906.volume.set` command is sent only after the value has actually changed.

### Channel slider gesture

- Center, Subwoofer and Rear also change **relatively**: by vertical finger movement, up is louder, down quieter. A tap does not move the slider to the finger position.
- The grab area is the whole channel column (label, value and slider), not just the narrow slider track.
- About 120 px of movement corresponds to the full 0–100 range — slower than the slider track itself, which makes precise adjustment in steps of 2 easier.
- `z906.level.set` is sent only after the value has actually changed; the rules for waiting for the snapshot are the same as for the ring.

The exact current Home Assistant entities are in `../integrations/z906.md`. The firmware does not use them directly; the bridge on the Home Assistant side maps them to the MQTT contract.

## States after wake-up and during actions

- Touch wakes the remote. A light connecting screen may be visible for up to about 2–3 s.
- The control screen becomes interactive only after a new snapshot in response to `remote.sync.request`.
- After a tap, the UI shows a pending state locally, but the persistent value is only the snapshot response.
- When data is missing or there is a connection error, do not pretend to show the current state.

### State appearance (mockup)

- **Connecting to home:** full screen with a house symbol in orbits, a title and three steps `Wi-Fi · MQTT · Synchronizacja` (Synchronisation). No tiles or device values in the background.
- **No connection to home:** the same composition with a no-network icon, a step that ended in error, a message that the remembered state is not shown, and a large `Ponów` (Retry) button.
- **Pending:** the tapped control pulses with a purple outline; the selection moves only after the snapshot. A slider follows the finger, and its value pulses until confirmed.
- **No confirmation:** after the timeout the control returns to the previous value and briefly signals an error with a red outline.
- **Device unavailable:** the TV or Z906 panel is greyed out and inactive, with a "Telewizor niedostępny" (TV unavailable) / "Z906 niedostępny" (Z906 unavailable) card in the middle. The bottom tab bar still works; the unavailable device's power switch in the header is disabled.

## Reference mockup

An interactive working version is in the repository:

```text
docs/ux/mockup/lg-tv-remote-ui.html
```

It opens directly in a browser (icons from Lucide via CDN). Below the remote screen there is a "Symulacja stanów" (State simulation) panel for toggling wake-up, connection error, device unavailability and missing action confirmation.

Do not copy its HTML or JavaScript into the firmware. It is used to evaluate appearance and interaction; the production implementation is written in C++/LVGL.
