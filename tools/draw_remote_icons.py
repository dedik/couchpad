#!/usr/bin/env python3
"""Rysuje ikony zakładki Pilot w pixel arcie (na razie tylko aplikacja na Androida).

Ten sam styl co tools/draw_icons.py: kształty wypełnione rampą jednego koloru, światło
z lewej góry, ciemny kontur i cień.

* `remote.png` — ikona zakładki, siatka 48,
* `key_<id>.png` — ikony klawiszy pilota, siatka 24: źródło, cofnij, menu, home,
  klawiatura, głośnik, wyciszenie.

Wynik: docs/ux/mockup-pulpit/assets/pixel/ (źródło) i kopia `px_<nazwa>.png`
w phone/native/app/src/main/res/drawable-nodpi/.

Uruchomienie:  tools/.venv/bin/python tools/draw_remote_icons.py [--preview plik.png]
"""

from __future__ import annotations

import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from draw_icons import RAMPS, WHITE, Canvas, ramp  # noqa: E402

from PIL import Image  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "ux" / "mockup-pulpit" / "assets" / "pixel"
PHONE = ROOT / "phone" / "native" / "app" / "src" / "main" / "res" / "drawable-nodpi"

RAMPS["white"] = ["#5a6274", "#8790a6", "#b4bccd", "#d7dce8", "#eef1f7", "#ffffff"]


def fin(c: Canvas) -> Canvas:
    c.outline()
    c.drop_shadow()
    return c


# ============================== zakładka (48) ==============================

def remote():
    """Pilot TV: grafitowy korpus, czerwone zasilanie, krzyżak z bursztynowym OK, klawisze."""
    c = Canvas()
    c.rect(14, 2, 33, 45, "graphite", 3, radius=4)
    c.rect(16, 4, 31, 43, "graphite", 2, bevel=False, radius=3)
    c.line(15, 6, 15, 40, ramp("graphite", 5))      # połysk lewej krawędzi
    # zasilanie
    c.disc(19.5, 8, 2.2, "red", 3)
    c.set(18, 7, ramp("red", 5))
    # dioda
    c.set(28, 7, ramp("amber", 5))
    c.set(27, 7, ramp("amber", 3))
    # krzyżak: pierścień ze strzałkami i OK
    c.ring(24, 19, 7.2, 4.4, "steel", 3)
    c.disc(24, 19, 3.6, "amber", 3.4)
    c.set(23, 17, ramp("amber", 5))
    for x, y in ((24, 13), (24, 25), (18, 19), (30, 19)):
        c.set(x, y, ramp("steel", 5))
    # klawisze: dwa rzędy po trzy, kołyski VOL / P
    for row, y in enumerate((29, 33)):
        for x in (18, 23, 28):
            c.rect(x, y, x + 2, y + 1, "steel", 3 if row == 0 else 2)
    c.rect(18, 37, 21, 42, "steel", 3, radius=1)
    c.rect(26, 37, 29, 42, "steel", 3, radius=1)
    c.set(19, 38, ramp("steel", 5))
    c.set(27, 38, ramp("steel", 5))
    return fin(c)


# ============================== klawisze (24) ==============================

def key_source():
    """Wejście: ekran z bursztynową strzałką wchodzącą od lewej."""
    c = Canvas(24)
    c.rect(7, 4, 21, 17, "graphite", 2, radius=1)
    c.vgrad(9, 6, 19, 15, "screen", 4.4, 1.6)
    c.set(10, 7, ramp("screen", 5))
    c.rect(12, 18, 16, 19, "graphite", 3, bevel=False)
    c.rect(2, 10, 11, 11, "amber", 4, bevel=False)
    c.poly([(10, 6.5), (15, 10.5), (15, 11.5), (10, 15.5)], "amber", 4)
    c.set(3, 10, ramp("amber", 5))
    return fin(c)


def key_back():
    """Cofnij: bursztynowa strzałka zawracająca w lewo."""
    c = Canvas(24)
    c.ring(13, 12.5, 7.5, 4.5, "amber", 3.5)
    # tylko prawa połowa łuku i dolna belka do lewej
    for y in range(24):
        for x in range(0, 13):
            if 5 <= y <= 20:
                c.px[y][x] = None
    c.rect(6, 17, 13, 19, "amber", 3)
    c.rect(6, 5, 13, 7, "amber", 4)
    c.poly([(1, 6), (7, 1), (7, 11)], "amber", 4)
    c.set(5, 4, ramp("amber", 5))
    return fin(c)


def key_menu():
    """Menu Apple TV: grafitowy klawisz ze stalowym szewronem w lewo."""
    c = Canvas(24)
    c.disc(12, 12, 9.5, "graphite", 2.6)
    c.poly([(13.5, 6), (16, 8), (11.5, 12), (16, 16), (13.5, 18), (7, 12)], "white", 4)
    c.set(13, 7, WHITE)
    return fin(c)


def key_home():
    """Home Apple TV: płaski ekran na nóżce z niebieskim gradientem."""
    c = Canvas(24)
    c.rect(2, 4, 21, 17, "graphite", 2, radius=1)
    c.vgrad(4, 6, 19, 15, "screen", 4.6, 1.4)
    c.set(5, 7, ramp("screen", 5))
    c.set(6, 7, ramp("screen", 5))
    c.set(5, 8, ramp("screen", 5))
    c.rect(10, 18, 13, 19, "graphite", 3, bevel=False)
    c.rect(7, 20, 16, 21, "graphite", 3)
    return fin(c)


def key_keyboard():
    """Klawiatura: grafitowa płyta, stalowe klawisze, spacja."""
    c = Canvas(24)
    c.rect(1, 6, 22, 18, "graphite", 2, radius=1)
    for y in (8, 11):
        for x in range(3, 21, 3):
            c.rect(x, y, x + 1, y + 1, "white", 3)
    c.rect(6, 14, 17, 15, "white", 3)
    c.rect(18, 14, 19, 15, "amber", 4, bevel=False)
    return fin(c)


def _speaker(c: Canvas):
    c.rect(2, 9, 6, 15, "steel", 3)
    c.poly([(6, 9), (12, 3), (12, 21), (6, 15)], "steel", 3.6)
    c.line(12, 4, 12, 20, ramp("steel", 2))
    c.set(3, 10, ramp("steel", 5))


def key_volume():
    """Głośnik z dwiema bursztynowymi falami."""
    c = Canvas(24)
    c.ring(11, 12, 6.4, 4.9, "amber", 4)
    c.ring(11, 12, 10.6, 9.1, "amber", 3.4)
    for y in range(24):
        for x in range(0, 14):
            c.px[y][x] = None
    _speaker(c)
    return fin(c)


def key_mute():
    """Głośnik z czerwonym krzyżykiem."""
    c = Canvas(24)
    _speaker(c)
    for i in range(7):
        for d in (0, 1):
            c.set(14 + i + d, 8 + i, ramp("red", 4 if i < 3 else 3))
            c.set(20 - i + d, 8 + i, ramp("red", 4 if i < 3 else 3))
    return fin(c)


ICONS = dict(remote=remote, key_source=key_source, key_back=key_back, key_menu=key_menu,
             key_home=key_home, key_keyboard=key_keyboard, key_volume=key_volume, key_mute=key_mute)


def preview(images: dict[str, Image.Image], path: pathlib.Path) -> None:
    cell = 104
    sheet = Image.new("RGBA", (cell * len(images), cell), (20, 22, 28, 255))
    for i, image in enumerate(images.values()):
        k = 96 // image.width
        sheet.alpha_composite(image.resize((image.width * k, image.height * k), Image.NEAREST), (i * cell + 4, 4))
    sheet.save(path)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    images = {name: draw().image() for name, draw in ICONS.items()}
    for name, image in images.items():
        image.save(OUT / f"{name}.png")
        if PHONE.is_dir():
            shutil.copyfile(OUT / f"{name}.png", PHONE / f"px_{name}.png")
        print(f"  {name}.png  {image.width}x{image.height}")
    if "--preview" in sys.argv:
        preview(images, pathlib.Path(sys.argv[sys.argv.index("--preview") + 1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
