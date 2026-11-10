# Apple TV — Home Assistant model

[Polski](appletv.md)

_Read-only inspection from 2026-10-31 via the Home Assistant REST API (`/api/states` and
`media_player.browse_media` for `apps`). Integration paired._

The remote does not use these identifiers directly — it talks via the `multimedia_remote/v1` contract
([ADR 0007](../architecture/adr/0007-apple-tv-via-home-assistant.en.md)). The table is a reference
for the bridge in `homeassistant/multimedia_remote.yaml` and for the tiles in
`components/ui/screens/screen_appletv.cpp`.

## Pairing

1. **Settings → Devices & services → Add integration → Apple TV.**
2. Select the device and enter the PINs shown on the TV (Companion, AirPlay).
3. On the Apple TV: **Settings → AirPlay and HomeKit → Allow Access: Anyone on the Same Network**,
   or leave "Only People Sharing This Home" if HA is in the same Apple home.

## Entities

| Function | Home Assistant entity | Notes |
| --- | --- | --- |
| Player, apps, power | `media_player.living_room_apple_tv` | attributes `app_id`, `app_name`, `source_list`; `turn_on`/`turn_off` |
| Remote | `remote.living_room_apple_tv` | only `remote.send_command`; its `turn_on`/`turn_off` does not put the device to sleep |
| Keyboard active | `binary_sensor.living_room_apple_tv_keyboard_focus` | `on` when a text field is shown on the Apple TV |
| Config entry | `<integration entry id>` | required by `apple_tv.set_keyboard_text`; changes after re-pairing |

`remote.living_room_apple_tv` has the "on" state only while the integration is connected to the Apple TV.
The bridge derives `appletv.available` from it: without a connection, commands fail in HA with the error
`Unable to send commands, not connected`, so the remote shows that there is no fresh state.

**Hung connection (2026-10-31).** The integration disconnected at 21:46 and did not come back on its own,
even though the Apple TV was responding on the network (ports 49153 and 7000 open). Reloading the config
entry helped (Devices & services → Apple TV → Reload). The log showed no cause;
if it happens again, enable debug logging for `pyatv` and `homeassistant.components.apple_tv`.

**Silent hang and the power watchdog (2026-11-10).** A second variant: `remote.…` still "on",
nothing in the log, but `media_player.…` stuck on "paused" for over 3.5 h while the Apple TV had
long been asleep. `turn_off` returned without error in a few ms and sent nothing. The remote saw
the Apple TV as on, so the power button only ever sent "off" and could not wake it. Reloading
the entry immediately showed the real `off` state. So `appletv.power.set` now goes through the
`script.multimedia_remote_atv_power` script: after `media_player.turn_on/off` it waits up to 8 s
for the requested state; if it does not arrive, it logs a warning (`multimedia_remote`), reloads
the integration entry (`homeassistant.reload_config_entry`) and repeats the command, unless the
fresh state already matches. When the remote was showing a stale "on", the first tap only
refreshes the state to "off" — the second one wakes the Apple TV.

## Apps on the tiles

Reading: **Developer tools → States → `media_player.…`**. Open each app on the
Apple TV and copy `app_id` and `app_name` exactly, character for character. Also check under which
name the app appears in `source_list`.

| Tile | `app_name` | entry in `source_list` | `app_id` (bundle id) | Launching |
| --- | --- | --- | --- | --- |
| Infuse | Infuse | `Infuse` | `com.firecore.infuse` | `play_media` — confirmed 2026-10-31 |
| YouTube | YouTube | `YouTube` | `com.google.ios.youtube` | `play_media` (not tested separately) |
| WP Pilot | — | `Pilot WP` | `pl.pilotwp.appletv` | `play_media` (not tested separately) |

Note: the WP Pilot app appears on the Apple TV as **"Pilot WP"**. Names in `source_list`
may contain non-breaking spaces (e.g. `App Store`), so we compare them byte for byte.

The full list of this Apple TV's apps (21) is available via `media_player.browse_media`
with `media_content_type: apps`, `media_content_id: apps`; a child's `media_content_id` is the bundle id.

In the "Launching" column, record what worked in Developer tools:

- `media_player.play_media` with `media_content_type: app`, `media_content_id: <app_id>` (preferred);
- or `media_player.select_source` with `source: <entry from source_list>`.

## Hidden Siri Remote gestures

The HA integration (`remote.send_command`) only knows a single tap and a hold
(`hold_secs` ≥ 1 → `InputAction.Hold`). A double tap can be composed of two
taps with no delay. Verified on this Apple TV 2026-10-31:

| Gesture | Effect | HA call | Result |
| --- | --- | --- | --- |
| Home ×2 | app switcher | `command: home`, `num_repeats: 2`, `delay_secs: 0` | works |
| Home — hold | Control Centre | `command: home`, `hold_secs: 1` | works |
| OK — hold | icon context menu | `command: select`, `hold_secs: 1` | works |
| Menu — hold | home screen | `command: top_menu` | not tested |

On the remote, the same gestures sit where they are on the Siri Remote: the Home button recognises ×2
(300 ms window) and hold (400 ms), the centre of the touchpad — hold. The contract carries
them as `appletv.navigate` with `"action": "double" | "hold"`.

Available, untested: `play_pause`, `skip_forward`, `skip_backward`, `channel_up`,
`channel_down`, `guide`, `screensaver`, `control_center`. Not available via HA: Siri,
simultaneous Home + Menu (restart).

## Checks before enabling the bridge

- [x] `remote.send_command` with `command: right` moves the selection on the Apple TV (2026-10-31).
- [x] `media_player.turn_off` puts the Apple TV to sleep, `media_player.turn_on` wakes it (2026-10-31).
      **Do not use `remote.turn_off/turn_on`:** it only changes the state in HA (the entities show
      `off`), while the Apple TV keeps running.
- [x] `media_player.play_media` with a bundle id launches the app (Infuse, 2026-10-31).
      **Note:** the `app_id` attribute did not switch to Infuse for at least 8 s —
      HA refreshes it from "now playing". The active-tile highlight on the remote may
      therefore show the previous app.
- [x] On the YouTube search field the `binary_sensor` goes `on`, and
      `apple_tv.set_keyboard_text` types the text, including Polish characters ("Kanał Zero", 2026-10-31).
