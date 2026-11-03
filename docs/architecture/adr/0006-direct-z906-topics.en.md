# ADR 0006 — The remote talks to the Z906 directly over MQTT

[Polski](0006-direct-z906-topics.md)

- **Date:** 2026-10-30
- **Status:** accepted
- **Amends:** [ADR 0002](0002-home-assistant-boundary.en.md) with respect to audio

## Problem

ADR 0002 established that the panel is a Home Assistant client: it publishes only the versioned
`multimedia_remote/v1` contract, and translation to entities is handled by a bridge on the HA side.
That bridge was built and works.

The command path through it looks like this:

```text
pilot → broker → Home Assistant → broker → Z906 controller
```

With the volume dial, where the finger expects an instant response, the detour through Home
Assistant is audible. Every hop means another round of queuing and another automation
to execute.

## Decision

For **audio**, the remote talks directly to the Z906 controller's topics:

```text
pilot → broker → Z906 controller
```

The controller has its own stable topics and publishes its full state. The remote subscribes to them and publishes
commands on its `command` topic. Home Assistant keeps working — it still sees the same
controller and the same topics, so its automations and entities stay unchanged.

## What it costs

**The remote knows another device's internal protocol.** Changing the controller or its topics
requires a firmware change. Until now such a change would have ended at an HA automation.

**Two devices control the same hardware independently.** The controller publishes its state after every
change, so the remote will learn about someone else's command — but conflict resolution no longer has
a single place.

**TV, scenes and shortcuts are left without a source.** They have no MQTT controller of their own, so for them
the path through Home Assistant remains the only sensible one — but until someone writes it, their
screens show "brak danych" (no data). The remote will eventually speak two languages at once, and that has to be
kept within a single interface.

## What stays unchanged

- The `multimedia_remote/v1` contract and the bridge in [`../../../homeassistant/`](../../../homeassistant/)
  remain documented and installed on the HA side — that is where the TV will return when its turn comes.
  The contract client disappears from the firmware: there is no point keeping a second connection that today
  serves no screen. It will come back together with the TV.
- The fresh-state rule from [ADR 0005](0005-fresh-state-after-wake.en.md): a touch does not move
  the selection; only the state received from the controller does.
- Ranges of 0–100 with a step of 2, and `Decode 5.1` as an indicator.
- Credentials and the controller identifier live in the device's NVS, not in the repository.

## Controller topics

The identifier has the form `z906_<mac>` and is entered on the device. State arrives
in separate topics, one value each, so the remote assembles the full picture from them.

| Topic | Value |
| --- | --- |
| `z906/<id>/availability` | `online` / `offline` |
| `z906/<id>/state/power` | `ON` / `OFF` |
| `z906/<id>/state/mute` | `ON` / `OFF` |
| `z906/<id>/state/volume` | 0–100 |
| `z906/<id>/state/centre` | 0–100 — **British spelling**, not `center` |
| `z906/<id>/state/subwoofer` | 0–100 |
| `z906/<id>/state/rear` | 0–100 |
| `z906/<id>/state/source` | one of the six input names |
| `z906/<id>/state/effect` | one of the four effect names |
| `z906/<id>/state/decode` | `ON` / `OFF` |

Commands go to `z906/<id>/command/<name>` with the same value the state expects.
The input and effect names match the lists in
[`../../integrations/z906.en.md`](../../integrations/z906.en.md) character for character, so there is nothing to translate.

The controller does not report a revision, so the remote numbers it: every received piece of state bumps
a counter and yields a new, complete snapshot.
