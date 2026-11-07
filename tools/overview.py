#!/usr/bin/env python3
"""Składa zbiorczy obraz z zrzutów ekranu do README.

Siatka w połowie skali, 8 px odstępu, tło z lewego górnego piksela pierwszego zrzutu —
tak jak dotychczasowe `docs/screenshots/overview*.png`.

    tools/.venv/bin/python tools/overview.py WYNIK.png KOLUMNY ZRZUT1.png ZRZUT2.png ...

Przykład (poziom, zakładka Dom):

    tools/.venv/bin/python tools/overview.py docs/screenshots/overview-home-landscape.png 2 \\
        docs/screenshots/landscape/{05-home,10-room,11-light,12-effects,13-bulb,14-drawers}.png
"""

from __future__ import annotations

import pathlib
import sys

from PIL import Image

GAP = 8
SCALE = 0.5


def main() -> int:
    if len(sys.argv) < 4:
        print(__doc__)
        return 2
    out = pathlib.Path(sys.argv[1])
    columns = int(sys.argv[2])
    images = [Image.open(path).convert("RGB") for path in sys.argv[3:]]
    width = round(images[0].width * SCALE)
    height = round(images[0].height * SCALE)
    rows = (len(images) + columns - 1) // columns
    sheet = Image.new("RGB", (columns * width + (columns - 1) * GAP,
                              rows * height + (rows - 1) * GAP),
                      images[0].getpixel((0, 0)))
    for index, image in enumerate(images):
        small = image.resize((width, height), Image.LANCZOS)
        sheet.paste(small, ((index % columns) * (width + GAP), (index // columns) * (height + GAP)))
    sheet.save(out, optimize=True)
    print(f"{out}  {sheet.width}x{sheet.height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
