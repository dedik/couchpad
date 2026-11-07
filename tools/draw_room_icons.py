#!/usr/bin/env python3
"""Rysuje ikony pomieszczeń i świateł dla ekranu Dom (ADR 0014) w stylu draw_icons.py.

* pomieszczenia — siatka 48 x 48: `room_<id>.png` (salon, kuchnia, przedpokoj, biuro,
  lazienka, sypialnia); kafelek Dom bierze je w 2 x, pasek pomieszczeń w 1 x,
* lampa wisząca — 32 x 32 w czterech stanach: `lamp_on`, `lamp_dim`, `lamp_off`, `lamp_na`,
* żarówka — 32 x 32: `bulb_on`, `bulb_off`.

Wynik trafia do docs/ux/mockup-pulpit/assets/pixel/ (źródło dla tools/gen_assets.py)
i do docs/ux/mockup-dom/assets/ (makiety).

Uruchomienie:  tools/.venv/bin/python tools/draw_room_icons.py
"""

from __future__ import annotations

import math
import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from draw_icons import RAMPS, SIZE, WHITE, Canvas, ramp  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "ux" / "mockup-pulpit" / "assets" / "pixel"
MOCKUP_DOM = ROOT / "docs" / "ux" / "mockup-dom" / "assets"

RAMPS["teal"] = ["#0c2f33", "#13504f", "#1d7a73", "#2fa39a", "#62cfc0", "#c8fff4"]
RAMPS["blue"] = ["#14203f", "#1f3466", "#2d4f96", "#4570c4", "#77a0e6", "#d0e0ff"]
RAMPS["white"] = ["#5a6274", "#8790a6", "#b4bccd", "#d7dce8", "#eef1f7", "#ffffff"]


def fin(c):
    c.outline(); c.drop_shadow(); return c


def salon():
    """Sofa z poduszkami i lampą stojącą obok."""
    c = Canvas()
    # lampa stojąca
    c.poly([(36, 6), (45, 6), (47, 15), (34, 15)], "amber", 4)
    c.line(36, 6, 44, 6, ramp("amber", 5))
    c.rect(40, 16, 41, 40, "graphite", 3, bevel=False)
    c.rect(37, 40, 44, 42, "graphite", 2)
    # oparcie
    c.rect(4, 18, 35, 30, "teal", 3, radius=3)
    # poduszki siedziska
    c.rect(8, 28, 19, 34, "teal", 4, radius=1)
    c.rect(20, 28, 31, 34, "teal", 4, radius=1)
    # podłokietniki
    c.rect(1, 24, 7, 37, "teal", 2, radius=2)
    c.rect(32, 24, 38, 37, "teal", 2, radius=2)
    c.rect(4, 35, 35, 37, "teal", 1, bevel=False)
    # poduszka dekoracyjna
    c.rect(10, 20, 17, 27, "amber", 3, radius=2)
    c.set(11, 21, ramp("amber", 5))
    # nóżki
    for x in (4, 34):
        c.rect(x, 38, x + 1, 41, "wood", 2, bevel=False)
    return fin(c)


def kuchnia():
    """Garnek z parą na kuchence."""
    c = Canvas()
    # para
    for i, x in enumerate((17, 24, 31)):
        for y in range(3, 14):
            dx = round(1.5 * math.sin((y + i * 2) / 2.0))
            c.set(x + dx, y, ramp("white", 4 if y > 6 else 5))
    # pokrywka
    c.rect(22, 13, 26, 15, "graphite", 3)
    c.rect(9, 16, 39, 18, "steel", 4, radius=1)
    # garnek
    c.rect(10, 19, 38, 33, "red", 3, radius=2)
    c.rect(12, 21, 14, 30, "red", 5, bevel=False)
    c.rect(5, 21, 9, 23, "graphite", 3)
    c.rect(39, 21, 43, 23, "graphite", 3)
    # kuchenka: płyta i palnik z płomieniem
    c.rect(3, 34, 45, 37, "graphite", 3)
    for x in range(14, 35, 3):
        c.set(x, 33, ramp("amber", 5)); c.set(x + 1, 33, ramp("red", 4))
    c.rect(3, 38, 45, 45, "white", 3)
    for x in (9, 17, 31, 39):
        c.disc(x, 41.5, 1.7, "graphite", 3, rim=False)
    return fin(c)


def przedpokoj():
    """Drzwi wejściowe z wieszakiem i wycieraczką."""
    c = Canvas()
    # framuga i drzwi
    c.rect(8, 3, 31, 43, "graphite", 3)
    c.rect(10, 5, 29, 43, "wood", 3, radius=1)
    c.rect(13, 8, 26, 19, "wood", 2)
    c.rect(13, 23, 26, 39, "wood", 2)
    c.rect(14, 9, 25, 18, "glass", 4, bevel=False)
    c.set(15, 10, WHITE)
    c.disc(26.5, 22, 1.4, "amber", 4, rim=False)
    # wieszak z płaszczem
    c.rect(34, 8, 46, 9, "wood", 3, bevel=False)
    for x in (36, 43):
        c.set(x, 10, ramp("steel", 4)); c.set(x, 11, ramp("steel", 4))
    c.poly([(37, 11), (42, 11), (44, 30), (35, 30)], "blue", 3)
    c.line(39, 12, 39, 29, ramp("blue", 1))
    c.line(37, 12, 36, 29, ramp("blue", 4))
    # wycieraczka
    c.rect(6, 44, 33, 46, "green", 2)
    return fin(c)


def biuro():
    """Monitor z kodem, klawiatura i kubek."""
    c = Canvas()
    c.rect(5, 4, 42, 30, "graphite", 2, radius=2)
    c.vgrad(8, 7, 39, 27, "screen", 2.2, 0.6)
    rows = [(10, 9, 22, "amber"), (12, 12, 30, "green"), (12, 15, 26, "steel"),
            (14, 18, 34, "green"), (12, 21, 22, "steel"), (10, 24, 17, "amber")]
    for x0, y, x1, col in rows:
        for x in range(x0, x1 + 1):
            if (x - x0) % 6 != 5:
                c.set(x, y, ramp(col, 4))
    c.set(18, 24, ramp("white", 5)); c.set(18, 23, ramp("white", 5))
    c.rect(20, 31, 27, 35, "graphite", 3)
    c.rect(14, 35, 33, 36, "graphite", 2)
    # biurko
    c.rect(1, 37, 46, 39, "wood", 3)
    c.rect(3, 40, 5, 46, "wood", 2, bevel=False)
    c.rect(42, 40, 44, 46, "wood", 2, bevel=False)
    # klawiatura
    c.rect(9, 33, 18, 36, "steel", 4)
    # kubek
    c.rect(36, 31, 41, 36, "red", 3, radius=1)
    c.rect(42, 32, 43, 34, "red", 2, bevel=False)
    return fin(c)


def lazienka():
    """Wanna z bąbelkami i prysznicem."""
    c = Canvas()
    # prysznic
    c.rect(38, 4, 39, 22, "steel", 4, bevel=False)
    c.rect(30, 4, 39, 5, "steel", 4, bevel=False)
    c.poly([(26, 6), (33, 6), (34, 9), (25, 9)], "steel", 3)
    for i, x in enumerate(range(26, 34, 2)):
        for y in range(11 + i % 2, 19, 3):
            c.set(x, y, ramp("screen", 4))
    # bąbelki
    for cx, cy, r in ((10, 18, 2.5), (16, 15, 3), (22, 18, 2.2), (14, 21, 2)):
        c.ring(cx, cy, r, r - 1, "white", 4)
        c.set(int(cx - r / 2), int(cy - r / 2), WHITE)
    # woda + wanna
    c.rect(4, 22, 36, 24, "screen", 4, bevel=False)
    c.rect(2, 24, 45, 26, "white", 5)
    c.rect(4, 27, 43, 37, "white", 3, radius=4)
    c.rect(7, 28, 9, 34, "white", 5, bevel=False)
    # nóżki
    c.rect(8, 38, 10, 42, "amber", 3)
    c.rect(37, 38, 39, 42, "amber", 3)
    return fin(c)


def sypialnia():
    """Łóżko z poduszką, kołdrą i księżycem."""
    c = Canvas()
    # księżyc i gwiazdki
    c.disc(37, 9, 5, "glass", 4, rim=False)
    c.disc(39.5, 7, 4.2, "graphite", 0, light=False, rim=False)
    for y in range(SIZE):
        for x in range(SIZE):
            if c.get(x, y) == ramp("graphite", 0):
                c.px[y][x] = None
    for sx, sy in ((26, 6), (44, 16), (30, 13)):
        c.set(sx, sy, ramp("glass", 5))
    # zagłówek
    c.rect(2, 16, 9, 42, "wood", 3, radius=2)
    # materac
    c.rect(9, 30, 45, 35, "white", 4)
    # poduszka
    c.rect(10, 24, 20, 30, "white", 4, radius=2)
    c.set(11, 25, WHITE)
    # kołdra
    c.poly([(18, 27), (44, 27), (46, 36), (17, 36)], "violet", 3)
    c.line(18, 27, 44, 27, ramp("violet", 5))
    for x in range(22, 44, 5):
        c.line(x, 29, x + 1, 35, ramp("violet", 2))
    # rama
    c.rect(9, 36, 46, 38, "wood", 2)
    c.rect(42, 30, 46, 42, "wood", 3, radius=1)
    c.rect(11, 39, 12, 42, "wood", 1, bevel=False)
    return fin(c)


def lamp(state, size=32):
    """Lampa wisząca: off / dim / on / na (niedostępna)."""
    c = Canvas(size)
    if state in ("on", "dim"):
        # promienie światła pod kloszem
        rng = 9 if state == "on" else 5
        lvl = 4.4 if state == "on" else 3.2
        for y in range(16, 16 + rng + 6):
            half = 5 + (y - 16) * 0.75
            for x in range(int(16 - half), int(16 + half) + 1):
                if (x + y) % 2 == 0 and abs(x - 16) < half - 1:
                    c.set(x, y, ramp("glass", lvl - (y - 16) / (rng + 6) * 2))
    # kabel
    c.rect(15, 0, 16, 6, "graphite", 3, bevel=False)
    shade = "amber" if state in ("on", "dim") else ("graphite" if state == "na" else "steel")
    base = 3 if state != "na" else 2
    c.poly([(12, 7), (19, 7), (26, 16), (5, 16)], shade, base)
    c.line(12, 7, 6, 15, ramp(shade, base + 2))
    c.rect(5, 16, 26, 16, shade, base - 1, bevel=False)
    # żarówka
    bulb = {"on": ("glass", 5), "dim": ("glass", 3.6), "off": ("steel", 2), "na": ("graphite", 2)}[state]
    c.disc(15.5, 18.5, 2.6, bulb[0], bulb[1], light=state not in ("on",), rim=False)
    if state == "on":
        c.set(14, 17, WHITE)
    c.outline()
    if state == "na":  # przekreślenie
        for i in range(0, 22):
            c.set(5 + i, 26 - i, ramp("red", 3))
    return c


def bulb(state, size=32):
    """Żarówka: on / off."""
    c = Canvas(size)
    on = state == "on"
    if on:
        for i, (dx, dy) in enumerate(((-11, 0), (11, 0), (0, -13), (-8, -9), (8, -9))):
            c.set(16 + dx, 12 + dy, ramp("glass", 5)); c.set(16 + int(dx * .85), 12 + int(dy * .85), ramp("glass", 4))
    c.disc(16, 12, 7.5, "glass" if on else "steel", 4.2 if on else 2.6, rim=False)
    if on:
        c.set(13, 8, WHITE); c.set(12, 9, WHITE)
    c.rect(13, 19, 18, 20, "glass" if on else "steel", 3 if on else 2, bevel=False)
    for y in (21, 23, 25):
        c.rect(12, y, 19, y + 1, "steel", 4 if y != 23 else 3, bevel=False)
    c.rect(14, 27, 17, 28, "graphite", 2, bevel=False)
    c.outline()
    return c


ROOMS = {"salon": salon, "kuchnia": kuchnia, "przedpokoj": przedpokoj,
         "biuro": biuro, "lazienka": lazienka, "sypialnia": sypialnia}


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    images = {f"room_{name}": draw().image() for name, draw in ROOMS.items()}
    images.update({f"lamp_{state}": lamp(state).image() for state in ("on", "dim", "off", "na")})
    images.update({f"bulb_{state}": bulb(state).image() for state in ("on", "off")})
    for name, image in images.items():
        image.save(OUT / f"{name}.png")
        if MOCKUP_DOM.is_dir():
            shutil.copyfile(OUT / f"{name}.png", MOCKUP_DOM / f"{name}.png")
        print(f"  {name}.png  {image.width}x{image.height}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
