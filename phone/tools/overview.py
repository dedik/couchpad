#!/usr/bin/env python3
"""Składa zbiorczy obraz aplikacji do README z zrzutów `tools/screenshots.sh`.

    ../tools/.venv/bin/python tools/overview.py portrait|landscape [light]

Pion: wszystkie ekrany, 6 kolumn po 300 px. Poziom: bez listy efektów i ustawień, 3 kolumny
po 640 px. Margines i odstęp 16 px, tło z narożnika poprzedniego obrazu (albo z pierwszego
zrzutu, gdy obrazu jeszcze nie ma). Wynik: docs/screenshots/overview-<orientacja>[-light].png.
"""

from __future__ import annotations

import pathlib
import sys

from PIL import Image

GAP = 16
LAYOUT = {"portrait": (6, 300, ()), "landscape": (3, 640, ("09-", "12-"))}


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] not in LAYOUT:
        print(__doc__)
        return 2
    mode = sys.argv[1]
    light = len(sys.argv) > 2 and sys.argv[2] == "light"
    shots = pathlib.Path(__file__).resolve().parent.parent / "docs" / "screenshots"
    src = shots / mode / ("light" if light else "")
    out = shots / f"overview-{mode}{'-light' if light else ''}.png"
    cols, width, skip = LAYOUT[mode]

    files = [p for p in sorted(src.glob("*.png")) if not p.name.startswith(skip)]
    images = [Image.open(p).convert("RGB") for p in files]
    bg = Image.open(out).convert("RGB").getpixel((2, 2)) if out.exists() else images[0].getpixel((0, 0))
    height = round(width * images[0].height / images[0].width)
    rows = (len(images) + cols - 1) // cols
    canvas = Image.new("RGB", (cols * width + (cols + 1) * GAP, rows * height + (rows + 1) * GAP), bg)
    for i, image in enumerate(images):
        row, col = divmod(i, cols)
        canvas.paste(image.resize((width, height), Image.LANCZOS), (GAP + col * (width + GAP), GAP + row * (height + GAP)))
    canvas.save(out, optimize=True)
    print(f"  {out.name} ({len(images)} ekranów)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
