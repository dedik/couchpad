# MQTT contract v1

[Polski](mqtt-contract.md)

## Principle

Home Assistant owns the real devices and their states. The remote does not publish to the existing, internal Z906 topics and makes no assumptions about entity names. It communicates through a dedicated remote contract, handled by an automation or bridge on the Home Assistant side.

Topics are versioned so that a future change does not break remotes in use.

```text
multimedia_remote/v1/<remote_id>/command       remote → Home Assistant
multimedia_remote/v1/<remote_id>/state         Home Assistant → remote
multimedia_remote/v1/<remote_id>/availability  both directions
```

`<remote_id>` is a stable identifier stored locally during the initial configuration. In examples we use `living-room-remote`; it is not a default, imposed name.

## Command

`command` topic: QoS 1, no retain. Exception: `appletv.navigate` is sent with QoS 0 — it is a momentary key press, and if redelivered later it would move the Apple TV without the user's involvement.

```json
{
  "id": "3f5575f7-1b5f-4f31-8129-82b2e5b2fca2",
  "action": "z906.volume.set",
  "args": { "percent": 42 }
}
```

Minimum actions for the first release:

```text
tv.power.set          { "on": true }
tv.navigate            { "key": "up|down|left|right|ok|back|home" }
tv.media               { "key": "play_pause|stop|rewind|fast_forward" }
tv.source.set         { "source": "exact option exposed by TV" }
tv.picture_mode.set    { "mode": "exact option exposed by TV" }
z906.power.set         { "on": true }
z906.mute.set          { "muted": true }
z906.volume.set        { "percent": 0..100 }
z906.level.set         { "channel": "center|subwoofer|rear", "percent": 0..100 }
z906.source.set        { "source": "..." }
z906.effect.set        { "effect": "..." }
ha.action              { "id": "named-action" }
remote.sync.request    {}
```

Apple TV actions ([ADR 0007](adr/0007-apple-tv-via-home-assistant.en.md)):

```text
appletv.power.set       { "on": true }
appletv.navigate        { "key": "up|down|left|right|select|menu|home", "action": "tap|double|hold" }
appletv.app.launch      { "app_id": "bundle id from appletv.apps[].id" }
appletv.keyboard.set    { "text": "..." }      replaces the whole text of the active field
appletv.keyboard.clear  {}
```

Air conditioning actions ([ADR 0009](adr/0009-climate-via-home-assistant.en.md)):

```text
climate.power.set             { "on": true }              turning on returns to the last mode
climate.mode.set              { "mode": "cool" }          identifier from climate.hvac_modes
climate.target.set            { "temperature": 22.5 }     within the range and step from the entity
climate.fan.set               { "mode": "medlow" }        from climate.fan_modes
climate.swing.set             { "mode": "Full" }          from climate.swing_modes
climate.swing_horizontal.set  { "mode": "Left" }          from climate.swing_horizontal_modes
```

Light actions ([ADR 0014](adr/0014-rooms-and-lights.en.md)). `id` is an identifier from the
`lights` section (main light or bulb); only the bridge knows the entities:

```text
light.power.set        { "id": "lampa", "on": true }
light.brightness.set   { "id": "lampa_lewa", "percent": 60 }     1..100, turns the light on
light.color_temp.set   { "id": "lampa", "kelvin": 3500 }         within min_kelvin..max_kelvin
light.effect.set       { "id": "lampa", "effect": "Cozy" }       from `effects`; "" = no effect
```

Radiator actions ([ADR 0018](adr/0018-radiators.en.md)). `id` is an identifier from the `radiators` section:

```text
radiator.power.set     { "id": "grzejnik_salon", "on": true }           turning on returns to the last mode
radiator.mode.set      { "id": "grzejnik_salon", "mode": "auto" }       from `modes`: off | heat | auto
radiator.target.set    { "id": "grzejnik_salon", "temperature": 20.5 }  within the entity's min..max
```

`action` in `appletv.navigate` is optional and defaults to `tap`. `double` and `hold` reproduce
the hidden gestures of the Siri Remote: Home ×2 opens the app switcher, holding Home opens Control
Center, holding `select` opens the context menu.

`appletv.navigate` is a momentary event: the bridge executes it without publishing a snapshot,
because a swipe across the trackpad produces several steps per second. `appletv.app.launch`
accepts only a bundle id from the `apps` list in the latest snapshot; the bridge rejects anything else.

Commands set state explicitly; we do not introduce `toggle` when the UI has no fresh, confirmed state. For the volume slider the client sends at most 10 commands/s and always sends the final value when the finger is lifted.

## State snapshot

`state` topic: QoS 1, retained. Each message contains a full, self-contained picture of the state and an increasing revision.

```json
{
  "revision": 41,
  "z906": {
    "available": true,
    "power": "on",
    "muted": false,
    "volume_percent": 42,
    "source": "input_2",
    "source_options": ["PC - analog 5.1", "RCA - stereo", "Optical 1", "Optical 2", "Coaxial", "AUX"],
    "center_percent": 56,
    "subwoofer_percent": 49,
    "rear_percent": 68,
    "effect": "stereo",
    "effect_options": ["Stereo 3D", "Stereo 2.1", "Stereo 4.1", "No effect"],
    "decode_5_1": false
  },
  "tv": {
    "available": true,
    "power": "on",
    "label": "LG OLED",
    "source": "HDMI 1",
    "source_options": ["HDMI 1", "HDMI 2", "TV", "Aplikacje"],
    "picture_mode": "Filmmaker Mode",
    "picture_mode_options": ["Vivid", "Standard", "Cinema", "Filmmaker Mode"]
  },
  "appletv": {
    "available": true,
    "power": "on",
    "app_id": "<bundle id of the active app>",
    "apps": [
      { "id": "<bundle id>", "label": "<name from source_list>" }
    ],
    "keyboard_active": false
  },
  "climate": {
    "available": true,
    "hvac_mode": "cool",
    "hvac_modes": ["off", "heat_cool", "cool", "heat", "fan_only", "dry"],
    "target": 22.0,
    "current": 24.0,
    "min": 16.0,
    "max": 31.0,
    "step": 0.5,
    "fan_mode": "medium",
    "fan_modes": ["auto", "quiet", "low", "medlow", "medium", "medhigh", "high", "strong"],
    "swing_mode": "Full",
    "swing_modes": ["off", "Full", "Upper", "Lower"],
    "swing_horizontal_mode": "Full",
    "swing_horizontal_modes": ["off", "Full", "Left", "Center", "Right"]
  },
  "lights": [
    {
      "room": "salon", "id": "lampa", "label": "Lampa",
      "available": true, "on": true, "dimmable": true,
      "brightness": 62, "kelvin": 3500, "min_kelvin": 2700, "max_kelvin": 6500,
      "effect": "", "effects": ["Cozy", "Relax", "TV time", "..."],
      "members": [
        { "id": "lampa_lewa", "label": "Lewa", "available": true, "on": true,
          "brightness": 70, "kelvin": 3500 }
      ]
    }
  ],
  "radiators": [
    {
      "room": "salon", "id": "grzejnik_salon", "available": true,
      "mode": "auto", "modes": ["off", "auto", "heat"],
      "target": 19.0, "min": 4.0, "max": 35.0, "step": 0.5,
      "heating": false, "battery": 100, "window_open": false,
      "next": { "at": "23:00", "target": 16.0 }
    }
  ],
  "shortcuts": [
    { "id": "movie-mode", "label": "Film", "enabled": true }
  ]
}
```

The `appletv` section:

- `apps` is the list of favourite apps configured in the bridge (at most 8), not the full
  `source_list`. The bridge publishes only those actually installed on the Apple TV.
- The app key is the bundle id. `label` is for display only, because the names
  in `source_list` depend on the app version and the system language.
- `app_id` is empty when the Apple TV is on the home screen or asleep.
- `keyboard_active` indicates that a text field is active on the Apple TV; the remote then slides
  out the keyboard.

The `climate` section:

- Modes and options are HA identifiers, not display names — the remote picks the Polish names.
  Each list has at most 8 entries; an empty list or a missing field hides the given control.
- `hvac_mode` equal to `off` means the air conditioning is off. In `fan_only` the target temperature has no effect.
- `current` is `null` when the room sensor does not report a value.
- Temperatures are in degrees; the remote rounds them to tenths.

The `lights` section ([ADR 0014](adr/0014-rooms-and-lights.en.md)):

- One entry per room with a main light (`room`: `salon`, `kuchnia`, `przedpokoj`, `biuro`,
  `lazienka`, `sypialnia`); the remote shows a room without an entry as "no light". The
  remote reads at most three entries and four bulbs in `members`.
- `brightness` in percent (0 when off), `kelvin` — current colour temperature, or 0 when the
  entity does not report it (off or in an effect). `min_kelvin` = 0 means no colour control.
- `dimmable: false` for a switch (`switch.*`) — on/off only.
- `effects` is the group entity's `effect_list` (at most 16), `effect` — current or empty.
- A missing section (bridge from before ADR 0014) disables lights; the rest of the remote works.

The `radiators` section ([ADR 0018](adr/0018-radiators.en.md)):

- One entry per thermostatic head, `room` as in `lights`. A room without an entry has no
  radiator card. The remote reads at most four entries.
- `mode` is the entity state: `off`, `heat` (fixed setpoint) or `auto` (the head's schedule);
  `modes` — the entity's `hvac_modes`. `target`, `min`, `max`, `step` in degrees.
- **No head temperature.** The head measures next to the radiator; the remote takes the room
  temperature from `indoor.rooms` in the weather message (the room sensor).
- `heating` — `hvac_action == heating` (valve open). `battery` — head battery percent, absent
  when unknown. `window_open` — the room's window contact sensor (not the head's `open_window`
  switch); absent when the room has no sensor or it is unavailable.
- `next` — the next point of the head's schedule that changes the temperature (`at` = `HH:MM`,
  `target` in degrees), computed by the bridge from today's and tomorrow's plan. Absent when
  the plan cannot be read. The schedule is view-only.
- A missing section (bridge from before ADR 0018) hides the radiator cards.

The remote ignores a snapshot whose revision is not greater than the last processed one. When there is no state, it shows the device as unavailable and does not guess its status.

The `source` and `effect` fields carry **the current option from their corresponding list**, not an internal
input identifier. The remote matches the value against `source_options` and `effect_options` to
highlight the right segment; a value outside the list highlights nothing. The example above shows
`input_2` for readability of the structure, but a correct payload repeats one of the options there.

`tv.picture_mode` is the current state reported by the TV and may change without user action, e.g. depending on the content. The UI updates it after every snapshot. The mode-change button is available only when `picture_mode_options` contains at least one option; option names are not hard-coded in the firmware.

`tv.source` and `tv.source_options` work the same way: the UI shows only the options delivered in a fresh snapshot.

**The TV uses this contract only for channels.** Since [ADR 0008](adr/0008-tv-via-glasshouse.en.md), state, navigation, source and picture mode go directly to the Glasshouse topics `<prefix>/...`. Only the channel shortcut goes through the contract, because Glasshouse switches channels only over HTTP:

```text
tv.channel.open        { "name": "Polsat" }               shortcut: name from the tuner list, ignoring case and spaces
tv.channel.open        { "name": "...", "id": "<id>" }    channel menu: the identifier from the `channels` list takes precedence
tv.channels.request    {}                                 request to republish the channel list
```

The bridge executes them without publishing a snapshot.

The bridge publishes the channel list on a separate topic, QoS 1, retained — after HA starts, every hour and after `tv.channels.request` (the remote sends it after every connection):

```text
multimedia_remote/v1/<remote_id>/channels      Home Assistant → remote
```

```json
{ "channels": [ { "number": "4", "name": "Polsat", "id": "1_23_4_4_2_3_8808" } ] }
```

The order is the tuner order, without radio stations. The list is not part of the snapshot: it is a few kilobytes and changes only after a channel scan. The remote shows only the list received from this topic and switches by `id`, because names can be duplicated. Program +/- does not go through this contract: it is a remote key sent directly to Glasshouse (`<prefix>/command/rcu channelup|channeldown`). The remaining `tv.*` actions and the `tv` section of the snapshot stay documented above for compatibility, but the bridge does not handle them and the firmware does not send them.

The bridge publishes the weather ([ADR 0011](adr/0011-weather-on-start-screen.en.md)) on a separate topic, QoS 1, retained — when the condition or temperature changes and after HA starts:

```text
multimedia_remote/v1/<remote_id>/weather       Home Assistant → remote
```

```json
{ "condition": "partlycloudy", "temperature": 5.5, "night": true,
  "indoor": { "temperature": 23.8, "humidity": 39,
              "rooms": [ { "room": "salon", "temperature": 22.6, "humidity": 39 } ] } }
```

`indoor` ([ADR 0015](adr/0015-indoor-average-on-start-screen.en.md)) is optional: the average
temperature (degrees, one decimal) and humidity (%, integer) of the room sensors, computed by the
bridge. Without the field the "at home" tile is hidden; without `humidity`, only its line.

`indoor.rooms` ([ADR 0017](adr/0017-room-climate-card.en.md)) is optional: the sensor reading of
each room separately. `room` is the identifier from the `lights` section (`salon`, `kuchnia`,
`przedpokoj`, `biuro`, `lazienka`, `sypialnia`), `temperature` in degrees with one decimal,
`humidity` in % (integer). Either reading field may be missing on its own (sensor unavailable);
a room without a sensor is not in the list, and the remote then hides the "In the room" card.
A room may share a sensor with a neighbouring one (kitchen: living room, hallway: office); the bridge decides.
The remote reads at most six entries.

`condition` is the state identifier of the `weather` entity in HA; the remote picks an icon and a Polish name for it. `night` says the sun is below the horizon (`sun.sun`): HA has a night variant only for a clear sky (`clear-night`), so e.g. `partlycloudy` at night is drawn by the remote with a moon. Weather is not part of the snapshot: it is content, not device state, so the remote shows the last one from memory immediately after wake-up, and hides it after 6 hours without an update.

The Home Assistant automation/bridge publishes a snapshot after every change of the TV, Z906, Apple TV, air conditioning, lights, radiators or a shortcut, after a restart, and after receiving `remote.sync.request`. A sync request after wake-up must generate a new revision after Home Assistant checks the current state; it must not merely resend the old retained message.

## Availability and errors

- The remote publishes `online` and an LWT `offline` on its availability topic, QoS 1 and retain.
- A sent command gets a local "oczekuje" (pending) state; it disappears after a new snapshot, a rejected connection or a timeout.
- A transport error must not block the UI or cause repeated, uncontrolled attempts to execute an action.
- After wake-up the control screen is available only after receiving a new snapshot generated in response to `remote.sync.request`. Before that, the connecting screen is shown, not the state from memory.

A change of topic, payload or semantics requires a new version (`v2`) or full backward compatibility and an ADR.
