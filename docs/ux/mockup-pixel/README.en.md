# Pulpit CRT — pixel variant

[Polski](README.md)

A third interface direction, alongside [`../mockup/`](../mockup/) and [`../mockup-pulpit/`](../mockup-pulpit/).
It replaces neither of them.

This is **the same system as Pulpit**: the same screens, the same class names, the same state logic and the same
gesture handling. Only the appearance layer changes — and this is also a test of whether Pulpit's
structure holds up at all. It turned out that it does: swapping the stylesheet was enough.

| File | Contents |
| --- | --- |
| `index.html` | mockup: five screens, full state simulation |
| `system.html` | component gallery in this theme |
| `pixel.css` | the entire difference from Pulpit |
| `pixel-icons.js` | tab icons, shared with Pulpit |

## How to view

```bash
cd docs/ux && python3 -m http.server 4173
```

Then `http://localhost:4173/mockup-pixel/index.html`.

## Rules of this theme

1. **No rounded corners and no blurred shadows.** The only exception is the volume ring, because
   a ring has to be a ring.
2. **Bevel instead of shadow.** Two pixels of light from the top and left, two pixels of shadow from the bottom and
   right. A pressed element has an inverted bevel — that is all the animation it needs.
3. **Dithering instead of gradient.** A 2 px checkerboard where Pulpit had a smooth transition.
4. **4 px grid.** All spacing and dimensions are multiples of it.
5. **CRT scanlines** across the whole screen: a dark stripe every two pixels, barely visible.
6. **Blinking instead of pulsing.** The pending state blinks in steps, because pixel art knows no half-tones.

Changes that are not mere decoration:

- **The volume ring is divided into thirty ticks.** The value can be counted by eye,
  not just estimated. The channel bars also have crossbars.
- **Posters are downloaded at 50 × 75 resolution** and enlarged without smoothing, so they are
  real pixel art, not a photo with a filter.
- **A telemetry bar** below the header shows MQTT status, revision number, signal strength and response
  time. In Pulpit this data lives on the diagnostics screen; here it is always on top, because
  that is the point of the variant for geeks. The revision number increases after each confirmed snapshot.

## Typeface and the trap I fell into

**Silkscreen** for labels, **VT323** for text and readouts. Both have the full set of Polish characters.

The first version had labels at 10 and 12 px, and the Polish characters fell apart — `GŁOŚNOŚĆ`
looked like `G.OŚNOŚĆ`. The cause was not missing glyphs but scale: **a pixel typeface may only be
enlarged by an integer factor**. Silkscreen is drawn on an 8 px grid, so it may be
given 8, 16 or 24 — at 10 px the scale is 1.25 and pixels land between screen pixels.
Characters with ogoneks and strokes fall apart first, because their details are the thinnest.

For the same reason this theme has no `letter-spacing`: extra spacing pushes characters off the
grid.

## What this means for the firmware

This variant is **cheaper to draw** than smooth Pulpit, not more expensive:

- A pixel typeface at its native size is 1 bit per pixel and zero antialiasing — a fraction of the memory
  Ubuntu needs at 4 bits.
- Bevels are two rectangles on the edges, not a blurred shadow that LVGL computes in software.
- Dithering is a 4 × 4 tile repeated in hardware, not a gradient.
- No rounded corners means no corner smoothing.

Only the ring comes out more expensive: thirty ticks are thirty arcs instead of one. In LVGL
this is done with a single `lv_arc` whose background is a tile with gaps, or with thirty `lv_obj`
placed once when the screen is built.

## What this variant does not change

The MQTT contract, the data model, the fresh-state-after-wake-up rule, the 0–100 ranges with step 2, or
the ban on guessing entities. All the caveats from the [Pulpit README](../mockup-pulpit/README.en.md)
apply here equally, including the fact that adopting this direction requires ADRs.
