# Pulpit — alternative UI direction

[Polski](README.md)

A second, independent design for the remote's interface. It **does not replace** the approved specification in
[`../tv-and-z906-ui.en.md`](../tv-and-z906-ui.en.md) or the mockup in [`../mockup/`](../mockup/) — both remain
unchanged. This is material for comparison and decision.

The same system also has a pixel variant: [`../mockup-pixel/`](../mockup-pixel/). It differs only in its
stylesheet, which incidentally proves that the structure described below holds regardless of
appearance.

| File | Contents |
| --- | --- |
| `index.html` | mockup: four screens, full state simulation |
| `system.html` | design system: tokens, components, states, mapping to LVGL |
| `pulpit.css` | the single source of style; both pages use the same file |
| `pixel-icons.js` | pixel-art tab icons, shared by both pages |

## How to view

The files are split up, so they need a server (`file://` will not load the stylesheet and logos):

```bash
cd docs/ux && python3 -m http.server 4173
```

Then `http://localhost:4173/mockup-pulpit/index.html` and `.../system.html`.

## The problem this direction solves

The approved mockup designs **two screens**. The architecture documentation eventually envisions six
(Home, TV, Z906, Home Assistant, Settings, Diagnostics). Adding more brings three
problems: the bottom bar with two tabs has no room to grow, the screen header changes meaning
depending on the view, and each screen gets its own shades and sizes.

Pulpit answers this with a single shell and a single set of components.

## Principles

**1. Three zones, always the same.** A 64 px header (screen identity + power for its device),
content, and an 80 px tab bar with icons only (since 2026-11-03; previously 88 and 96 px, with labels
and a status bar). 5 px of spacing above the header. A new screen fills only the middle zone.

**2. The header belongs to the current screen.** It shows the name, room and power of only the
device that is visible. The sound screen has no TV power switch, and screens without a device
(Home, More) have no power switch at all. The "two power switches on one bar" problem goes away.

**3. A fixed number of navigation slots.** Five tabs: `TV`, `APPLE TV`, `DŹWIĘK` (SOUND), `DOM` (HOME), `WIĘCEJ` (MORE).
At 480 px width this gives 86 px per tab, which is still a comfortable touch target. **This is the limit** —
a sixth screen does not get its own tab, only a row in `WIĘCEJ`. The bar never changes layout.

**4. One accent.** Amber means "active": power on, selected tab, value fill,
the OK button. Red has one meaning — mute and error. Green — confirmed
availability. The three Z906 channels share the same colour; they are distinguished by labels, not hues.

**5. Real pixels.** The mockup renders at 480 × 800, not scaled. The 4.3″ panel has approx.
8.6 px/mm, so the minimum touch target is 68 px (≈ 8 mm). With a mockup scaled to ~368 px it is easy to
design buttons that are 4 mm on the hardware and impossible to hit.

**6. Five states for every control.** Idle, selected, waiting for snapshot, no
confirmation, no fresh state. They follow directly from ADR 0005 and the MQTT contract: a tap never
moves the selection by itself — only the snapshot does.

**7. Continuous values change relatively.** The dial responds to finger rotation, the columns to vertical
movement. A tap does not move the value to the finger position.

## Apple TV screen

Built exclusively from system components, with three new ones:

- **A touchpad** instead of a D-pad, because that is how the real Apple TV remote works. It is **a circle
  240 px in diameter**, centred the same way as the D-pad on the TV screen — the same silhouette for the same role.
  The shape follows from the gesture: a strip spanning the whole screen gave a 2.7 : 1 ratio, i.e. three times less
  travel for vertical swipes than horizontal ones. A circle gives both directions the same room, and
  zones are determined by distance from the centre and angle, not rectangles. The surface is uniform: there is no
  filled button on it, because in this system the accent means state, not action — and a coloured block
  in the middle of a swipe area contradicts the gesture itself. Only what you are
  pressing right now lights up. `Menu` and `Home` sit at the sides of the circle instead of taking up a separate row.
- **A new-releases carousel** with 100 × 150 posters, scrolled horizontally with snapping to positions.
  Above it, a `Nowe filmy | Nowe seriale` (New movies | New series) toggle. Two separate carousels would take approx. 360 px of the 588
  available, and hiding them behind a button would take away what looks best on the screen.
  The title sits on the poster under a dimming overlay, so it takes no height. A tap opens a
  details window with a description and a play button.
- **Details window** — the first overlay component in the system, based on the same background dimming
  as the wake-up screen.

App shortcuts (Infuse, YouTube, WP Pilot) work like the channel shortcuts on the TV screen: for now
visual only, eventually they launch the app through the bridge.

### Library and posters

Item metadata goes over a separate MQTT topic, and **posters are downloaded over HTTP** — the state snapshot is
retained and must not be burdened with a list of movies. Emby scales the image on its side (`maxWidth`),
so the remote downloads the target 106 × 160 directly: approx. 10 KB as JPEG, approx. 36 KB after decompression.
Downloading and decoding are done by a worker task, never by the UI task.

**This requires a separate ADR**, because it splits the rule from ADR 0005. Device state must be fresh after
every wake-up. The library is content, not state — it may be cached and
shown immediately after wake-up, without waiting for the network. Without writing this difference down, someone will treat
one like the other.

The Emby API key does not go into the repository — it goes into the device configuration in NVS.

## Start screen

After boot and after every wake-up, the remote shows the **start screen** instead of the last
device screen — **only then**. Navigation does not know about it: tapping a tile closes it
until the next wake-up, and the tab bar stays unchanged (`TV | APPLE TV | DŹWIĘK | DOM |
WIĘCEJ`). Full screen, above the header and the tab bar:

- at the top a 56 px clock and the date with the room name;
- four tiles 2 × 2 (approx. 210 × 270 px): Telewizor (TV), Apple TV, Głośniki (Speakers), Dom (Home). The icon is the
  tab sprite at 4 × scale (96 px, each grid pixel as 4 × 4 screen pixels), below it the
  name at 26 px and a one-sentence state line with a dot: accent = on, grey = off, pulsing =
  connecting, red = no fresh state;
- at the bottom the home link state and `Ponów` (Retry) after an error;
- to the right of the clock, weather from HA (ADR 0011): a 64 px pixel-art icon, temperature to the whole
  degree and the condition name. This is content, not state, so it shows immediately from memory; at night
  `partlycloudy` has a moon instead of a sun.

**Why.** A control screen cannot show state from memory (ADR 0005), so today after
wake-up it is blocked by the "Łączenie z domem" (Connecting to home) overlay. The start screen controls nothing, so it
appears immediately, and synchronisation runs in the background; by the time a finger hits a tile, the state is usually
already there. Entering a device before the snapshot shows the overlay as before.

Adopting this in the firmware requires an ADR: it changes a detail of ADR 0005 (overlay only on device
screens) and replaces the "Ekran startowy" (Start screen) setting — the screen after wake-up is always the start screen.

## Home screen and air conditioning

The `DOM` screen starts with a **device tile**: 432 × 168, with no controls at all. It shows only
what you need to know before entering it — the operating mode and the target temperature (large number),
with the room temperature below. When the AC is on, the symbol and mode name have the accent; when off it is
grey; no fresh state replaces the values with a dash and outlines the tile in red. A tap
opens the subpage. The shortcuts (Light, Blinds, Climate, Camera) are gone: Climate was replaced by the tile, and
the rest had no content yet.

**The air conditioning subpage** is `widgets::make_page`: full screen above the tab bar, with
`Cofnij` (Back) on the left of the header and AC power on the right, both 64 px. The content matches the HA thermostat card for
`climate.living_room_ac`:

- **A 300 px dial**, a 270° arc over the 16–31 °C range, step 0.5. It changes by finger rotation, relatively, just
  like volume. Below the arc, `−` and `+` at 72 px each. Several taps make one command: it is sent 0.7 s
  after the last touch, and until then the value pulses with the accent. In fan mode and when
  off, the dial dims, because the target temperature has no effect then.
- **Operating mode** — five segments with icons: Auto, Cooling, Heating, Dry, Fan.
  Off has no segment, that is the power button's job; selecting a mode while the AC is off
  also turns it on (`climate.set_hvac_mode`), and power restores the last mode.
- **Fan speed, vertical and horizontal louvres** — three tiles with the current value. Fan speed has eight levels
  (`fan_modes`: Auto, Quiet, Low, Medium-low, Medium, Medium-high, High, Maximum),
  so selection is from a list in a bottom sheet, like the TV source and picture mode: a single column of 68 px rows,
  each level with an icon, as in HA. The louvres have shorter lists without icons. The sheet closes only after
  confirmation by a snapshot.

The single accent stays: cooling is not blue and heating is not orange — the mode is distinguished by icon
and label. The mockup has the range, step and option lists hard-coded; in the firmware they will come from the
entity attributes (`min_temp`, `max_temp`, `target_temp_step`, `hvac_modes`, `fan_modes`, `swing_modes`,
`swing_horizontal_modes`). This requires a new section in the MQTT contract, and therefore an ADR.

In the firmware: `ui/screens/screen_climate.cpp` and `widgets::Thermostat`. The 64 px readout has its own font
map (digits, decimal comma, dash), and the degree sign uses a smaller font beside it. Two differences from the
mockup: the mode segments are text-only, because the mockup's function icons do not exist yet, and the fan speed list
is a subpage with rows, like the TV source list, instead of a bottom sheet.

## Pixel-art tab icons

The tabs have colourful retro-style sprites. A button in the simulation panel switches them back
to plain line icons, should a comparison be needed.

The icons are original, not from a ready-made pack — none has a symbol that fits Apple TV alongside
the other four, and mixing sources breaks the family's consistency.

**Since 2026-11-03 this is a 48 × 48 grid** (`assets/pixel/*.png`), drawn by the script
`tools/draw_icons.py`: each material has a six-shade ramp (light from the top left, shadow
from the bottom right), plus highlights, a dark outline and a semi-transparent drop shadow — a style
modelled on detailed pixel-art sets. On a tab the icon is shown at 1 × scale (48 px),
on a start-screen tile at 2 × (96 px), always without smoothing. It is fixed in the script
(and the script is rerun) or directly in the PNG in a pixel editor. The old 24 × 24 grids in
`pixel-icons.js` remain only as a fallback for the component gallery.

Motifs: a CRT television with an antenna, a flat set-top box with a play triangle, a speaker column
with a woofer and a tweeter, a house with lit windows, and a three-by-three grid. The set-top box is
deliberately low and wide so it is not confused with the television. A shared palette and the system accent in a few
details bind them into one family.

**Labels stay.** The risk is not the icon style but removing the text: with five tabs,
two of which concern the television, an icon alone is not enough to tell them apart.

Because the sprite has its own colours, the selected tab is no longer marked by the icon's colour but by its background.

For the firmware this is actually a simplification: a 48 × 48 grid becomes an indexed image — at
1× in the tab bar, scaled 2× with nearest-neighbour on the start-screen tiles, without
antialiasing — cheaper than an icon font.

## Typeface

**Ubuntu**, in three weights: 400, 500 and 700. The design uses no other weight, so neither the
browser nor the firmware needs to synthesise bold. In the mockup the typeface comes from
Google Fonts; it is the only external dependency besides the icon library.

Two things speak in its favour in this project:

- **All ten digits have the same width.** Verified by measurement: 26.13 px at 46 px in
  weight 700, identical for every digit. So the volume readout and channel levels will not
  jump when the value changes, and no separate monospaced typeface is needed for them.
- **Full Polish characters**, so nothing needs to be drawn in.

### What this means for the firmware

Changing the typeface is cheap, because LVGL does not use TTF files at runtime anyway: `lv_font_conv`
converts the typeface into bitmaps **separately for each size**. Swapping the typeface means regenerating
those maps, with no changes to the screen code.

The cost is program memory. The design uses six sizes (11, 13, 15, 19, 26 and 46 px). A rough
estimate at 4 bits per pixel and a Latin set with Polish characters: text sizes a few to
a dozen or so kilobytes each, 26 px around twenty-something. The 46 px size is used only for the
volume readout, so a **digits-only subset** is enough for it — a few kilobytes instead of
several dozen. In total on the order of several dozen kilobytes out of 16 MB of memory, i.e. negligible.

The Ubuntu Font Licence allows embedding the typeface in a device. During bring-up it is worth checking whether
six sizes are really needed — going down to four is the simplest saving available
here.

## How to add a screen

1. Add an entry to `SCREENS` in `index.html`: name, room, icon, optionally the power key
   and the device identifier for the unavailability bar.
2. Add a `<section class="scr" data-scr="…">` in `.content`.
3. Build the content exclusively from components in `system.html`.
4. Add a tab or a row in `WIĘCEJ`.
5. Do not add colours or dimensions — if something is missing, add it to the tokens first.

## Differences from the approved specification

Adopting this direction requires an ADR, because it breaks four decisions:

- **Tabs.** The spec requires two equal tabs `TV | GŁOŚNIKI` (TV | SPEAKERS). Here there are five.
- **Apple TV and the library.** The spec does not provide for them. They add new MQTT commands (navigation,
  launching an app, playing an item), a new topic for library metadata, and downloading
  images over HTTP. Each of these requires an ADR and a contract update.
- **Z906 layout.** The spec approves "Soundstage" with orbital decoration and three channel colours. Here
  there is the same content (ring, sources, three channels, effect, `Decode 5.1`) in a uniform style,
  without decoration and without colour differentiation.
- **Header on the TV screen.** The spec places both power switches there (TV and Z906). Here Z906 power is
  only on the sound screen.

Unchanged: the MQTT contract, the data model, the fresh-state-after-wake-up rule, 0–100 ranges
with step 2, `Decode 5.1` as an indicator, and the ban on guessing LG entities.

## Data

The Infuse and YouTube logos come from the Simple Icons library. `assets/wppilot.svg` is the white wordmark
from the pilot.wp.pl header (symbol `#logo`, fetched 2026-11-03) — on the tile it sits like the TVP wordmarks.

**The mockup really downloads posters over HTTP**, from the random-photo service `picsum.photos`, at the target
size of 200 × 300. These are not the covers of those titles, just random photographs — the point is to
evaluate the carousel's appearance and at the same time rehearse the real path: a skeleton while downloading,
replacement once the image arrives, and a local fallback on error. One poster from there weighs approx. 8 KB, which is
exactly what I assume for Emby. The files in `assets/posters/` are only those fallback backgrounds.

The mockup works on simulated data. The values of sources, picture modes and effects match the lists
in [`../../architecture/mqtt-contract.en.md`](../../architecture/mqtt-contract.en.md) and
[`../../integrations/z906.en.md`](../../integrations/z906.en.md), but are not fetched from Home Assistant.
The channel shortcuts and the `DOM` screen are visual — they do not yet have counterparts in the contract.
