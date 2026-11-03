# ADR 0009 — Air conditioning via Home Assistant and the v1 contract

[Polski](0009-climate-via-home-assistant.md)

- **Date:** 2026-11-03
- **Status:** accepted
- **Supplements:** [ADR 0002](0002-home-assistant-boundary.en.md), [ADR 0007](0007-apple-tv-via-home-assistant.en.md)

## Problem

The `DOM` screen has an air-conditioning tile and its subpage (mockup: `docs/ux/mockup-pulpit`,
code: `ui/screens/screen_climate.cpp`), but nothing live stood behind them — they worked
only on mock data. The air conditioner has no MQTT controller of its own. In Home
Assistant it is a `climate` entity (Midea integration), which already has everything the screen
needs: modes, target and room temperature, range, step, fan speeds and louvres.

## Decision

Air conditioning takes the same path as the Apple TV: the
[`multimedia_remote/v1`](../mqtt-contract.en.md) contract, with translation to the entity handled by the bridge
in [`../../../homeassistant/`](../../../homeassistant/).

```text
pilot → broker → Home Assistant (climate.*) → klimatyzator
```

The contract gains a `climate` section in the snapshot and `climate.*` actions. The change is additive:
old clients ignore the unknown section, and the bridge ignores unknown actions. The version in the topic stays `v1`.

Agreed points:

- **Identifiers, not names.** Modes and options flow as HA identifiers (`cool`,
  `medlow`, `Full`). The screen picks the Polish names; an unknown identifier is shown as
  it arrived. The lists (`hvac_modes`, `fan_modes`, `swing_modes`, `swing_horizontal_modes`)
  come from the entity; the firmware does not hard-code them.
- **Range and step from the entity** (`min_temp`, `max_temp`, `target_temp_step`). Temperatures in JSON
  are numbers in degrees; the firmware keeps them in tenths.
- **Turning on returns to the last mode** (`climate.turn_on`); selecting a mode while the AC is off
  also turns it on (`climate.set_hvac_mode`). Turning off is `climate.turn_off`.
- **Temperature is one command per gesture.** The remote sends the final value: when the finger is
  lifted from the dial, or 0.7 s after the last tap on −/+.
- **Snapshot after a change of state and of selected attributes** (target and room temperature,
  fan, louvres) — not after every integration attribute, because Midea changes many of them at once.

## Consequences

- AC state arrives over the same HA client as the Apple TV. Losing the broker connection greys out
  both; a missing `climate` section in the snapshot (a bridge predating this decision) greys out only the AC.
- The remote's snapshot grew by approx. 1.1 kB. Events from transport clients carry only the state of
  their own device and are created off the MQTT task's stack (cf. handoff, 2026-11-03).
- The entity is hard-coded in the bridge (`climate.living_room_ac`). A second AC means a second section and an ADR.
