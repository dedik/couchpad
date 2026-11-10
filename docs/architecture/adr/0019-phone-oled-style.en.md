# ADR 0019 — OLED style in the Android app

[Polski](0019-phone-oled-style.md)

- **Date:** 2026-11-10
- **Status:** accepted
- **Extends:** [ADR 0012](0012-light-and-dark-theme.en.md), [ADR 0016](0016-android-app.en.md)

## Problem

The remote's dark style was designed for the board's IPS screen: a black background, cards in
greys from `#15171C` up, an orange accent filling large areas, near-white text. The Android app
(ADR 0016) mirrors it 1:1, but it also runs on phones with OLED/AMOLED screens, where:

- large grey cards cover most of the screen, so the benefits of black (contrast, lower power)
  are lost;
- greys just above black render differently on every panel — at low brightness they crush into
  black or turn purple, and they smear while scrolling;
- solid orange on large areas (OK, wide buttons) and near-white text glare in a dark room
  (halation of bright letters on black);
- the faintest captions (`faint #626A78`, 3.9:1 contrast) are hard to read.

Android does not reliably report whether the screen is OLED, so the choice cannot be automatic.

## Decision

- **A separate variant of the dark style, in the app only:** *Settings → Phone → Dark style on
  this phone: standard / OLED*. Standard by default. The `oled` preference
  (`Preferences.oled`, `shared_prefs/pilot.xml`) is local to the phone and never reaches MQTT
  or the firmware. It applies whenever the dark style is in effect (dark, or by hours during the
  dark hours); the light style is unchanged.
- **`OledPalette`** (`phone/native/.../ui/theme/Theme.kt`):
  - background, cards and controls are pure black inside (`surface`, `surface2` = `#000000`);
    the shape is drawn by the `line #2E3137` outline alone;
  - elements that the regular style separates from the background only by their fill (effect,
    climate-mode and radiator tiles, Z906 channel cards, the header icon) get a `C.edge`
    outline — transparent in the regular styles;
  - grey remains only on press (`C.pressed` = `surface3 #24272B`) and on arc tracks;
  - text at about 75% white (`#C2C5CA`), `dim #8C929B`, `faint #777D87` (contrast ≥ 4.5:1);
    the arc knob uses the text colour instead of pure white; neutral greys without a blue tint;
  - accent `#F57C14` (headroom for saturating display modes such as Vivid); large accent areas
    — the TV OK key and wide buttons — become a 16% soft fill with an accent outline and label
    (`Palette.softAccent`); power and arcs stay solid (small elements);
  - the plate under channel logos is black like the cards (`Palette.logoPlate`).
- The palette flags (`outlined`, `softAccent`, `logoPlate`) default to the existing styles, so
  standard dark and light look as before.
- Along the way, the system navigation bar now follows the current style's background (it used
  to be dark always, including in the light style).

## Consequences

- This is a deliberate exception to "the app mirrors the panel 1:1" (AGENTS.md): it concerns
  only a setting on the Phone page, which exists only in the app anyway. The panel has an IPS
  screen and does not need the variant.
- New code drawing cards or controls must use the palette tokens: `C.surface`/`C.surface2` for
  fills, `C.line` or `C.edge` for outlines, `C.pressed` for the pressed state. A hard-coded grey
  breaks the OLED variant.
- Text colour does not replace screen brightness: the remote enforces the brightness from
  *Settings → Display → Brightness* (100% by default) regardless of the system slider. At 100%
  on OLED even 75%-white text is bright — on OLED phones a lower brightness (40–60%) or night
  mode is recommended.
- The README screenshots show the standard style; the OLED variant has no separate set.
