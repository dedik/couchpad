# Mockup — landscape orientation 800 × 480

[Polski](README.md)

A clickable mockup of the remote's interface in landscape. The firmware's landscape layouts
were built from it ([ADR 0013](../../architecture/adr/0013-landscape-orientation.en.md)). It uses
the pixel icons and logos from [`../mockup-pulpit/assets/`](../mockup-pulpit/assets/) and the
portrait screenshots from [`../../screenshots/`](../../screenshots/) for comparison, so it needs
a server rooted at the `docs` directory:

```bash
cd docs && python3 -m http.server 4174
```

Then `http://localhost:4174/ux/mockup-landscape/index.html`.

## Switches above the mockup

| Switch | What it shows |
| --- | --- |
| Pasek (rail) | side tab rail on the left or on the right |
| Styl (theme) | dark and light |
| TV: wybór | source and picture mode in the header (firmware) or in the column |
| Z906: wybór | source and effect in the header (firmware) or above the sliders |
| Klima | climate card variants on the Home screen; firmware uses "C szybkie" |
| Podstrona klimy | climate sub-page variants; firmware uses "P1" |
| Kafle trybu | 3 × 2 or a large mode tile on the left; firmware: "duży = wybrany" (selected is large) |
| Zasilanie | solid 64 px circle or the smaller 48 px one (firmware) |
| porównaj z pionem | the same screen in portrait next to the mockup |
| cele dotyku | outlines of the touch areas |

The mockup is for judging look and behaviour. Its HTML and JavaScript are not copied into the
firmware.
