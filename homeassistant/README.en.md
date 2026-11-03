# Home Assistant side bridge

[Polski](README.md)

The remote speaks only the versioned `multimedia_remote/v1` contract and knows neither the Z906
entities nor the internal topics of its controller. The translation is handled by the
[`multimedia_remote.yaml`](multimedia_remote.yaml) package. The reason for this split is described
in [ADR 0002](../docs/architecture/adr/0002-home-assistant-boundary.en.md), and the message format in
the [MQTT contract](../docs/architecture/mqtt-contract.en.md).

The network connection is direct: the remote connects to the MQTT broker over TCP. Home Assistant
is not on the network path — it only contributes content, because it is the one that knows the
state of the devices.

**The broker does not have to run on the same machine as Home Assistant.** Both are its clients
and they meet on topics, not on an address. In the remote you enter the address of the **broker**,
not of Home Assistant; Home Assistant gets there through its MQTT integration. The only
requirement is that this integration points to the same broker — you can check this under
Settings → Devices & services → MQTT.

## What the package does

| Element | Role |
| --- | --- |
| `counter.multimedia_remote_revision` | increasing snapshot revision; the remote ignores older ones |
| `script.multimedia_remote_publish` | builds the full snapshot and publishes it as retained |
| automation "on Z906, Apple TV or AC change" | new snapshot after every entity change; AC — state and attributes shown on the remote |
| automation "on startup" | snapshot after a Home Assistant restart |
| automation "execute command" | receives the `command` topic and sets entities, including `climate.*` ([ADR 0009](../docs/architecture/adr/0009-climate-via-home-assistant.en.md)) |
| automation "Apple TV navigation" | `appletv.navigate` → `remote.send_command`, without a snapshot |
| script and automation "TV channel list" | retained tuner list on `.../channels`: on startup, every hour, on `tv.channels.request` |
| automation "TV channel" + `rest_command` | `tv.channel.open` → tuner list and `open` on Glasshouse, without a snapshot ([ADR 0008](../docs/architecture/adr/0008-tv-via-glasshouse.en.md)) |

## Installation

1. Enable packages in `configuration.yaml` if you do not have them yet:

   ```yaml
   homeassistant:
     packages: !include_dir_named packages
   ```

2. Copy `multimedia_remote.yaml` to the `packages/` directory in your Home Assistant configuration.

3. **Replace `living-room-remote`** with the identifier entered on the remote
   (`WIĘCEJ → MQTT → Identyfikator pilota` (MORE → MQTT → Remote identifier)). It appears in three
   topics and must be identical on both sides, because it forms the topic.

3a. **Apple TV** ([ADR 0007](../docs/architecture/adr/0007-apple-tv-via-home-assistant.en.md)):
   the entities, `atv_config_entry` and `atv_favourites` are already filled in according to
   [`docs/integrations/appletv.md`](../docs/integrations/appletv.en.md). After re-pairing the
   integration, the config entry identifier changes — it then has to be replaced.

3c. **Air conditioning** ([ADR 0009](../docs/architecture/adr/0009-climate-via-home-assistant.en.md)):
   the entity in the `&climate` anchor (`climate_entity: climate.living_room_ac`) and in the triggers
   of the "on … change" automation. The bridge takes modes, range, step and options from the
   entity's attributes. After changing the package, `check_config` and reloading scripts and
   automations is enough.

3b. **TV channels** ([ADR 0008](../docs/architecture/adr/0008-tv-via-glasshouse.en.md)): add
   the TV address and the Glasshouse token (from its `config.json`) to `secrets.yaml`:

   ```yaml
   glasshouse_channels_url: http://<tv-address>:8080/api/home/channels
   glasshouse_open_url: http://<tv-address>:8080/api/home/open
   glasshouse_authorization: Bearer <token>
   ```

   Without them the package will not load. `rest_command` is reloaded separately.

3d. **Your entities.** The package uses example names. Replace them with your own (Settings →
   Devices & services → Entities):

   | In the package | What it is |
   | --- | --- |
   | `switch.z906_power`, `switch.z906_mute` | Z906 power and mute |
   | `number.z906_volume`, `number.z906_center`, `number.z906_subwoofer`, `number.z906_rear` | volume and channel levels |
   | `select.z906_input`, `select.z906_effect` | input and effect |
   | `binary_sensor.z906_decode_5_1` | Decode 5.1 indicator |
   | `media_player.living_room_apple_tv`, `remote.living_room_apple_tv`, `binary_sensor.living_room_apple_tv_keyboard_focus` | Apple TV (`&appletv` anchor) |
   | `WPISZ_ID_WPISU_APPLE_TV` | placeholder (Polish for "enter Apple TV entry ID"): the config entry identifier of the Apple TV integration |
   | `climate.living_room_ac` | air conditioning (`&climate` anchor) |
   | `weather.forecast_home` | weather on the start screen |

   The Z906 entities appear in several templates — the simplest way is to replace them with a
   search across the whole file, e.g.:

   ```bash
   sed -i 's/number\.z906_volume/number.my_z906_volume/g' multimedia_remote.yaml
   ```

   If the package is already running in your HA, keep your own copy outside the repository or
   carry changes over by hand — overwriting it with the example names will disable the bridge.

4. Check the configuration and reload: Developer tools → YAML → Check configuration, then reload
   scripts, automations and counters, or restart Home Assistant.

5. Create a separate MQTT account for the remote, so as not to share credentials with other
   clients, and enter it on the device. Access credentials are not kept in this repository.

## Testing without the remote

Developer tools → Actions → `script.multimedia_remote_publish`. Then, in the MQTT integration
settings, listen to the `multimedia_remote/v1/+/state` topic — a full snapshot with an increasing
revision should arrive.

You can test a command by publishing to `multimedia_remote/v1/<identifier>/command`:

```json
{ "id": "test-1", "action": "z906.volume.set", "args": { "percent": 30 } }
```

## Scope and limits

The package handles the **Z906**, **Apple TV** and **air conditioning**. It does not publish the
`tv` section, so on the TV screen the remote will show a bar about missing fresh state — this is
correct behaviour, not a fault. The remote reads the Z906 directly from the controller anyway
(ADR 0006); from this package it takes the `appletv` and `climate` sections. The LG entities are
not yet confirmed, and guessing their names is forbidden in this project.

We publish `source` and `effect` as the **current option from the list**, not as an internal input
identifier. The remote matches this value against `source_options` and `effect_options` to
highlight the correct segment; an identifier such as `input_2` would not match any option and the
screen would highlight nothing. The example in the contract can be misleading in this respect.

Out-of-scope commands are written to the log as a warning instead of silently disappearing.
