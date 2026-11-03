# ADR 0005: Fresh state after wake-up

[Polski](0005-fresh-state-after-wake.md)

- **Status:** accepted
- **Date:** 2026-10-29
- **Supersedes:** ADR 0004

## Requirement

After the remote is touched to wake it, the control screen may appear roughly 2–3 s later, but it must present a new, confirmed state of the TV and the Z906.

## Decision

By default the device enters a power-saving mode. A touch starts the sequence Wi-Fi → MQTT → `remote.sync.request`. After this request Home Assistant publishes a new, full snapshot. Only receiving it unlocks the control screen.

During wake-up the screen shows a dedicated connecting view, not stale device tiles. If synchronisation fails, it shows a connection error and an option to retry.

## How it turned out

Done for audio. A touch wakes the backlight and immediately sends `IntentKind::RequestSync`;
the state returns to the connecting phase, which is cleared only by a snapshot with a new revision or by a 15 s limit.

The Z906 controller does not know the sync request, but it publishes its state as retained, and in MQTT 3.1.1
every SUBSCRIBE receives retained messages anew — so resubscribing is that request
([ADR 0006](0006-direct-z906-topics.en.md)).

Full power saving is left for later: today only the backlight turns off, while Wi-Fi and MQTT
stay up. Putting the radio to sleep would lengthen wake-up from a fraction of a second to about eight seconds and requires
confirming the hardware touch wake-up path.

## Consequences

- The state visible to the user does not depend on data from before sleep.
- Default power consumption will be lower than in connected standby.
- Achieving the 2–3 s time depends on Wi-Fi strength, MQTT and the Home Assistant integration; tests will establish the actual target and the behaviour when the limit is exceeded.
- The hardware touch wake-up path must be confirmed for the chosen power mode.
