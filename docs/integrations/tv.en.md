# LG OLED77C11LB TV via Glasshouse

[Polski](tv.md)

Status as of 2026-11-02, Glasshouse 0.138.0, webOS (C1, 2021). Decision: [ADR 0008](../architecture/adr/0008-tv-via-glasshouse.en.md).

## Configuration on the remote

Screen `WIĘCEJ → MQTT`, row "Telewizor" (TV): the topic prefix from the MQTT tab in the
Glasshouse panel, `lgtv` by default (set automatically). Leaving it empty disables the TV.

The broker, user and password are shared with the Z906 and Apple TV. The broker has no ACL, so the remote's account
sees the `lgtv/...` topics without changes. The `lgtv` account on the broker belongs to the TV.

Channel shortcuts need three entries in `secrets.yaml` on the Home Assistant side
(see [`homeassistant/README.en.md`](../../homeassistant/README.en.md)).

## What works

| On the remote | Command | Confirmation |
| --- | --- | --- |
| D-pad, OK | `command/rcu` `up/down/left/right/ok` | none (momentary) |
| Back | `command/rcu` `back` | none |
| TV menu | `command/rcu` `settings` (LG quick settings) | none |
| Source | `command/input` `livetv` / `hdmi1`–`hdmi4` | `state/application/app` |
| Picture mode | `command/picture_mode` `<id>` | `state/picture/pictureMode` |
| Polsat, TVN, TVP 1, TVP 2 | v1 contract `tv.channel.open` → HA → HTTP `POST /api/home/open` | none (momentary) |
| Channel menu (last tile) | v1 contract `tv.channel.open` with `id` from the `channels` list → HA → `open` | none (momentary) |
| P + / P − | `command/rcu` `channelup` / `channeldown` (requires the Glasshouse patch, ADR 0008) | none (momentary) |
| VOL + / VOL − | Z906 volume (`z906/<id>/command/volume`), step 2 | Z906 state |

Sources on this TV: `TV` (Live TV), `HDMI 1`, `HDMI 2`, `PC` (HDMI 3), `Apple TV` (HDMI 4).
The HDMI names come from the TV settings and arrive in telemetry, so renaming an input on the TV changes
the label on the remote.

Picture mode opens a list on a subpage, because there can be eight modes, and names like "Dolby Vision
Cinema Bright" do not fit in a row of segments.

## Channels

Verified 2026-11-02: the tuner list has 55 DVB-T channels. All four shortcuts find
their channel by name: `TVP1` (1), `TVP2` (2), `Polsat` (4), `TVN` (5) — also with the bridge's
template, run in the HA template engine against the real list. Switching from TVN to
Polsat (`POST /api/home/open` from a computer) took 0.8 s; `GET /api/home/now` confirmed the channel.

## What is missing

- **Power.** Wake-on-LAN is disabled on the TV; the button stays non-functional.
- **Volume.** Audio goes over optical to the Z906; the TV volume entities are `unavailable`.
- **Playback, keyboard, apps.** Glasshouse has them (`command/playback`,
  `command/keyboardText`, `command/launch_app`), the TV screen does not yet.
