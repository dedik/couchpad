# ADR 0010 — Start screen after power-on and wake

[Polski](0010-start-screen-after-wake.md)

- **Date:** 2026-11-03
- **Status:** accepted
- **Supplements:** [ADR 0005](0005-fresh-state-after-wake.en.md)

## Problem

ADR 0005 requires that after wake only new, confirmed state is shown. Until now that meant
a "Łączenie z domem" (Connecting to home) overlay over the whole remote until the snapshot: a touch woke the screen, and then
for a second or several there was nothing to touch. On top of that, the remote returned to the screen selected
in the settings ("Ekran po starcie" (Screen after start)), not to where the user happened to be heading.

## Decision

After start and after every wake, the remote shows a **start screen**: a clock, the date with the room
name, four large tiles ("Telewizor" (TV), "Apple TV", "Głośniki" (Speakers), "Dom" (Home)) and the state of the link to home.
Only then — navigation does not know about it, and the tab bar stays `TV | APPLE TV | DŹWIĘK | DOM
| WIĘCEJ`. Tapping a tile closes the start screen and opens the device screen.

The start screen controls nothing, so it may appear **immediately**, before the snapshot.
Synchronisation per ADR 0005 runs unchanged, in the background. The tiles say "łączenie…" (connecting…) until the state
arrives, and then summarise the fresh state in one sentence (e.g. "głośność 42 · Optical 1" (volume 42 · Optical 1))
— never the state from before sleep.

The connecting overlay remains, but only on device screens: it is visible when someone enters a
device before the snapshot arrives. Design: `docs/ux/mockup-pulpit` (README, "Ekran startowy" (Start screen)).

## Consequences

- Waking is noticeably faster: by the time the finger hits a tile, the state is usually already there.
- The "Ekran po starcie" setting goes away (`Pref::StartScreen`, `Pref::LastScreen`).
- Subpages open before sleep (selection list, AC, settings) close on wake
  — otherwise they would cover the device screen selected from the tile.
- A link error state is already visible on the start screen, with a `Ponów` (Retry) button.
