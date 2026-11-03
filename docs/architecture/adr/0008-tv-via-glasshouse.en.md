# ADR 0008 — The TV directly via Glasshouse

[Polski](0008-tv-via-glasshouse.md)

- **Date:** 2026-11-02
- **Status:** accepted
- **Amends:** [ADR 0002](0002-home-assistant-boundary.en.md) with respect to the TV,
  and closes the announcement in [ADR 0006](0006-direct-z906-topics.en.md) that the TV would return via Home Assistant

## Problem

ADR 0006 left the TV without a source: the LG had no MQTT controller of its own, so the only
path seemed to be a bridge in Home Assistant. Since then the TV has been running
[Glasshouse](https://github.com/rorygallagher2024/glasshouse) — a server running on webOS
itself, which publishes state and accepts commands over MQTT, on the same broker as the Z906.
Home Assistant sees it via MQTT discovery (entities `*_<tv_name>_*`).

An HA bridge for the TV would mean the same path that ADR 0006 rejected for audio:

```text
pilot → broker → Home Assistant → broker → Glasshouse
```

plus changes to the HA package and an HA restart for every new action.

## Decision

The remote speaks MQTT only, as with the Z906 and Apple TV. State and buttons go directly
to Glasshouse; channel shortcuts go via the v1 contract to the bridge in Home Assistant, because Glasshouse
switches channels only over HTTP:

```text
pilot → broker → Glasshouse                          (state, navigation, input, picture mode)
pilot → broker → Home Assistant → HTTP → Glasshouse  (channel shortcuts: tv.channel.open)
```

Home Assistant loses nothing: it sees the same topics and the same entities. The remote does not know the TV's
address or the Glasshouse token — only HA knows them (`secrets.yaml`).

## Topics

The prefix is set in Glasshouse (MQTT tab), `lgtv` by default. The remote keeps it in NVS
together with the broker credentials.

| Topic | Direction | Value |
| --- | --- | --- |
| `<p>/status` | TV → | `online`, `off` (standby, server running), `asleep`, `offline` |
| `<p>/telemetry` | TV → | JSON ~5 kB every 10 s, retained: `inputs`, `picture.modes`, `picture.mode_raw`, `app`, `input_source`, `powerState` |
| `<p>/state/application/app` | TV → | short identifier of the foreground app: `hdmi4`, `livetv`, `netflix`… |
| `<p>/state/picture/pictureMode` | TV → | picture mode identifier: `standard`, `dolbyHdrCinema`… |
| `<p>/state/power/systemOn` | TV → | `true` / `false` |
| `<p>/command/rcu` | → TV | `up`, `down`, `left`, `right`, `ok`, `back`, `home`, `settings`, `channelup`, `channeldown` |
| `<p>/command/input` | → TV | `livetv`, `hdmi1`–`hdmi4` |
| `<p>/command/picture_mode` | → TV | identifier from `picture.modes[].value` |

The `state/...` topics arrive immediately after a change; telemetry only every 10 s, so without them
confirmation of a source selection would not fit within the wait limit (3 s). From telemetry
we take the option lists and fallback values until the immediate topic has arrived.

Names and identifiers are two different things: the screen shows "Apple TV" or "Dolby Vision
Cinema", while the command takes `hdmi4` or `dolbyHdrCinema`. `TvState` therefore carries, alongside the option
lists, parallel identifier lists (`source_ids`, `picture_mode_ids`), and the UI intent carries the
identifier in `payload`. The firmware hard-codes neither.

Picture modes depend on the content (SDR, HDR, Dolby Vision). The remote shows only those
the TV will accept right now.

## Channel +/-

P+/P− are the LG remote keys `KEY_CHANNELUP` (402) and `KEY_CHANNELDOWN` (403). The
`networkinput/test/sendKeyCode` service, through which Glasshouse sends keys, accepts them —
verified 2026-11-02 with `luna-send` on the TV, the channel jumped from 6 to 7. Glasshouse 0.138.0, however,
has a closed `RCU_KEYS` key list without them, so `channelup` and
`channeldown` need to be added (a local patch on the TV and a report to the author). Until then P+/P− on the remote
ends with an `unknown key` rejection on the Glasshouse side and changes nothing.

## Channels via Home Assistant

Glasshouse exposes neither channel buttons nor digits over MQTT. It does, however, have an HTTP API for its
Home view:

- `GET /api/home/channels` — the tuner list `{id, number, name, radio}`;
- `POST /api/home/open` with `{"kind":"ch","id":"<id>"}` — `tv/openChannel`: tunes the channel
  and brings Live TV to the foreground, without a separate source change.

The remote sends `tv.channel.open {"name": "Polsat"}` on the v1 contract's `command` topic. The bridge
fetches the tuner list, matches the name ignoring case and spaces (first match,
skipping radio) and calls `open`. The list is fetched for every shortcut, because the identifier
contains multiplex parameters and changes after a channel rescan.

The requests require an `Authorization: Bearer <token>` header (the token from Glasshouse's `config.json`)
and, for POST, `Content-Type: application/json`. They are made by a `rest_command` in the bridge
package; the addresses and the token are in Home Assistant's `secrets.yaml`.

The command is momentary and does not publish a snapshot: the remote reads the TV state directly from Glasshouse.

### Channel menu

The bridge publishes the full list on the retained topic `multimedia_remote/v1/<id>/channels`
(number, name, identifier; no radio) at startup, every hour and on `tv.channels.request`,
which the remote sends after connecting. Selecting from the menu sends `tv.channel.open` with `id` — names
repeat (TVP World at 33 and 91), so the bridge does not look up by name.

The list lives outside `DeviceSnapshot` (`domain::ChannelList`, up to 96 channels, approx. 7 kB): one copy
in the HA client under a mutex, one in the snapshot source, which the UI reads from. On-demand
reading over HTTP from HA was considered and rejected: it would require an HA token on the remote and a second channel,
and the list has to sit in RAM anyway while the menu is open.

## What it costs

- **The remote knows the Glasshouse protocol**, just as it knows the Z906 controller's protocol. A change of topics
  in a new Glasshouse version requires a firmware change. Verified on version 0.138.0.
- **The channel HTTP API is an internal API of the Home view**, not described in the MQTT documentation.
  It may change without notice.
- **A third MQTT connection** (`pilot-tv-<prefix>`), alongside the Z906 and Home Assistant. On the
  same remote account: the broker has no ACL, so a separate user is not needed. The
  `lgtv` account belongs to the TV and the remote does not use it.
- **Channels depend on Home Assistant.** Without HA the channel shortcuts do not work; the rest of the TV screen does.
- **Power stays non-functional.** Turning on from standby requires Wake-on-LAN, and the TV
  has it disabled (`wakeOnLan: false` in telemetry). Glasshouse's `button.power_on` only works
  with Active Standby. Left for a separate decision.

## What stays unchanged

- The link phase is determined by the Z906 (ADR 0006). A missing Glasshouse greys out only the TV screen.
  Loss of the broker by the TV client greys out the screen only after 5 s: on weak Wi-Fi the client
  drops the connection every few minutes and comes back after about 3 s, and the broker then resends the retained state.
  Commands issued during that gap are lost and end with no confirmation, not a silent retry.
- The fresh-state rule from [ADR 0005](0005-fresh-state-after-wake.en.md): after wake the remote
  resubscribes to the topics and the broker resends retained values; until then the TV state is unavailable.
- The `multimedia_remote/v1` contract additively gains the `tv.channel.open` action; the `tv` section
  of the snapshot and the other `tv.*` actions are not used.
