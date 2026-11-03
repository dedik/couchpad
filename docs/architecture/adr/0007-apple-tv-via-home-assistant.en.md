# ADR 0007 — Apple TV via Home Assistant and the v1 contract

[Polski](0007-apple-tv-via-home-assistant.md)

- **Date:** 2026-10-31
- **Status:** accepted
- **Supplements:** [ADR 0002](0002-home-assistant-boundary.en.md), [ADR 0006](0006-direct-z906-topics.en.md)

## Problem

The Apple TV screen has a trackpad, Menu and Home buttons, app tiles and power
in the header, but nothing behind them talks to the device. Unlike the Z906, the Apple TV has no MQTT
controller of its own. It speaks the Companion/AirPlay protocol, which requires HAP pairing:
SRP, Ed25519, ChaCha20-Poly1305 and mDNS discovery.

## Decision

The Apple TV is handled by the `apple_tv` integration in Home Assistant (pyatv). The remote talks to it
via the [`multimedia_remote/v1`](../mqtt-contract.en.md) contract, and translation to entities is handled by
the bridge in [`../../../homeassistant/`](../../../homeassistant/).

```text
pilot → broker → Home Assistant (apple_tv / pyatv) → Apple TV
```

The contract gains an `appletv` section in the snapshot and `appletv.*` actions. The change is additive:
old clients ignore the unknown section, and the bridge ignores unknown actions. The version in the topic stays `v1`.

The remote now speaks two languages at once, as ADR 0006 anticipated:

- Z906 — directly with the controller (`z906/<id>/…`);
- Apple TV — via the v1 contract (`multimedia_remote/v1/<remote_id>/…`).

`LiveSource` (`components/application`) combines both connections into a single snapshot. It numbers the revision
itself, so `AppController` and the "ignore a snapshot that is not newer" rule stay unchanged.

## Apps are identified by bundle id

App names in the Apple TV `source_list` can differ from the marketing names and depend on the language.
The tile key is therefore the bundle id (`app_id` in HA), and the name is used for display only.
Bundle ids are read from the real device (see
[`../../integrations/appletv.en.md`](../../integrations/appletv.en.md)), not guessed.

## Why not natively on the ESP32

- HAP pairing and the encrypted Companion channel mean several thousand lines of cryptography that
  sdkconfig does not have today (ChaCha20, Poly1305, HKDF), plus an mDNS component.
- Apple changes these protocols between tvOS versions; pyatv keeps up, firmware without OTA does not.
- Pairing keys would be yet another secret in NVS.

## What it costs

**Navigation goes through Home Assistant**, i.e. through the same extra hop that
in ADR 0006 was audible on the volume dial. We mitigate it as follows:

- `appletv.navigate` is a momentary event: the bridge does not publish a snapshot after it;
- navigation is handled by a separate automation that does not queue behind the Z906;
- the remote limits trackpad movement to a few steps per second.

If that is not enough, the fallback is a small pyatv ↔ MQTT bridge. The contract stays
the same, so only the server side changes.

**Two MQTT connections after wake.** A missing Home Assistant does not block audio: the connecting
overlay is cleared by the Z906 snapshot, and the Apple TV screen shows "brak świeżego stanu" (no fresh state) until
a new snapshot arrives from HA (ADR 0005).

**Keyboard without read-back.** HA can set the text of a field on the Apple TV, but does not return it.
The remote always sends the full text (`appletv.keyboard.set`) rather than appending characters.

## What stays unchanged

- The Z906 goes directly (ADR 0006).
- The "Nowe filmy" (New movies) and "Nowe seriale" (New series) sections stay local; their data requires a separate ADR.
- The remote identifier (`remote_id`) and broker credentials live in NVS.
