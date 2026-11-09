#!/usr/bin/env python3
"""Rysuje ikony urządzeń i czujników Home Assistanta w pixel arcie.

Ten sam styl co tools/draw_icons.py i tools/draw_room_icons.py: kształty wypełnione rampą
jednego koloru, światło z lewej góry, ciemny kontur i cień.

* urządzenia — `dev_<id>.png`, siatka 48 (studzienka karty 1x/2x): grzejnik, termostat,
  klimatyzator, roleta, gniazdko, czujnik, odkurzacz, zamek, wentylator, domownik, scena,
  start (kafelki pulpitu),
* odczyty czujników — `sensor_<id>.png`, siatka 24 (karta klimatu pokoju w 2x):
  temperatura (termometr), wilgotność (kropla).

Zapas na przyszłe encje: na panel trafiają dopiero, gdy tools/gen_assets.py je wybierze.

Wynik: docs/ux/mockup-pulpit/assets/pixel/ (źródło) i kopia w docs/ux/mockup-dom/assets/.

Uruchomienie:  tools/.venv/bin/python tools/draw_device_icons.py [--preview plik.png]
"""

from __future__ import annotations

import math
import pathlib
import shutil
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from draw_icons import RAMPS, WHITE, Canvas, ramp  # noqa: E402

from PIL import Image  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "ux" / "mockup-pulpit" / "assets" / "pixel"
MOCKUP_DOM = ROOT / "docs" / "ux" / "mockup-dom" / "assets"

RAMPS.setdefault("teal", ["#0c2f33", "#13504f", "#1d7a73", "#2fa39a", "#62cfc0", "#c8fff4"])
RAMPS["white"] = ["#5a6274", "#8790a6", "#b4bccd", "#d7dce8", "#eef1f7", "#ffffff"]
RAMPS["ice"] = ["#123a5c", "#1d5f8f", "#2f8cc4", "#5bb6e8", "#a6dcf7", "#eefaff"]


def fin(c: Canvas) -> Canvas:
    c.outline()
    c.drop_shadow()
    return c


# ============================== urządzenia (48) ==============================

def radiator():
    c = Canvas()
    # pipe
    c.rect(3, 12, 44, 14, "steel", 3, bevel=False)
    for i in range(7):
        x = 5 + i * 5
        c.rect(x, 10, x + 3, 38, "white", 3, radius=1)
        c.set(x, 11, ramp("white", 5))
    c.rect(4, 36, 42, 38, "white", 2, bevel=False)
    # valve head (TRV)
    c.rect(41, 16, 46, 26, "white", 4, radius=1)
    c.rect(42, 18, 45, 19, "red", 3, bevel=False)
    # legs
    c.rect(7, 39, 8, 43, "graphite", 3, bevel=False)
    c.rect(38, 39, 39, 43, "graphite", 3, bevel=False)
    # heat waves
    for i, x in enumerate((10, 20, 30)):
        for k in range(5):
            c.set(x + (1 if k % 2 else 0), 2 + k, ramp("red", 4 - (k % 2)))
    return fin(c)

def thermostat():
    c = Canvas()
    c.rect(6, 6, 41, 41, "white", 3, radius=6)
    c.ring(24, 24, 15, 11.5, "amber", 3)
    # gap at bottom
    for y in range(32, 41):
        for x in range(18, 31):
            if math.hypot(x + .5 - 24, y + .5 - 24) > 11 and y > 33: c.set(x, y, ramp("white", 3))
    c.disc(24, 24, 10.5, "graphite", 2, light=False)
    # digits "21"
    seg = ramp("amber", 5)
    for y in range(19, 30): c.set(25, y, seg); c.set(26, y, seg)
    for x in (18, 19, 20, 21): c.set(x, 19, seg); c.set(x, 24, seg); c.set(x, 29, seg)
    for y in range(19, 24): c.set(21, y, seg)
    for y in range(24, 30): c.set(18, y, seg)
    c.disc(36, 12, 2, "red", 4, rim=False)
    return fin(c)

def blinds():
    c = Canvas()
    c.rect(4, 4, 43, 43, "wood", 2)
    c.rect(7, 7, 40, 40, "screen", 4, bevel=False)
    c.disc(32, 30, 4, "glass", 4, rim=False)
    for i in range(7):
        y = 7 + i * 3
        c.rect(6, y, 41, y + 1, "cream", 4 if i % 2 == 0 else 3, bevel=False)
        c.line(6, y + 2, 41, y + 2, ramp("cream", 1))
    c.rect(5, 4, 42, 6, "graphite", 3)
    c.line(38, 7, 38, 30, ramp("steel", 4))
    c.disc(38, 31, 1.5, "steel", 4, rim=False)
    return fin(c)

def plug():
    c = Canvas()
    c.rect(6, 6, 41, 41, "white", 3, radius=4)
    c.disc(24, 24, 13, "white", 4)
    c.disc(18.5, 24, 2.2, "graphite", 1, light=False)
    c.disc(29.5, 24, 2.2, "graphite", 1, light=False)
    c.rect(23, 11, 25, 14, "steel", 3, bevel=False)
    c.rect(23, 34, 25, 37, "steel", 3, bevel=False)
    c.disc(37, 10, 1.6, "green", 4, rim=False)
    return fin(c)

def sensor():
    c = Canvas()
    # thermometer
    c.rect(14, 4, 21, 32, "white", 4, radius=3)
    c.rect(16, 12, 19, 32, "red", 3, bevel=False)
    c.disc(17.5, 36, 7, "red", 3)
    for y in (8, 12, 16, 20, 24):
        c.line(22, y, 24, y, ramp("steel", 3))
    # drop
    c.poly([(34, 14), (40, 26), (39, 31), (34, 34), (29, 31), (28, 26)], "ice", 3)
    c.disc(34, 28, 5.6, "ice", 3)
    c.set(31, 25, WHITE); c.set(31, 26, WHITE)
    return fin(c)

def vacuum():
    c = Canvas()
    c.disc(24, 26, 19, "graphite", 3)
    c.disc(24, 26, 14, "graphite", 4, rim=True)
    c.rect(19, 9, 29, 13, "steel", 3, radius=2)
    c.disc(24, 26, 5, "teal", 3)
    c.disc(24, 26, 2, "teal", 5, rim=False)
    c.disc(15, 21, 1.4, "green", 4, rim=False)
    c.line(9, 38, 4, 43, ramp("steel", 4)); c.line(39, 38, 44, 43, ramp("steel", 4))
    return fin(c)

def lock():
    c = Canvas()
    c.ring(24, 18, 12, 7.5, "steel", 3)
    c.rect(8, 20, 39, 43, "amber", 3, radius=3)
    c.rect(8, 20, 39, 21, "amber", 5, bevel=False)
    c.disc(23.5, 29, 3.4, "graphite", 1, light=False)
    c.rect(22, 30, 25, 37, "graphite", 1, bevel=False)
    return fin(c)

def fan():
    c = Canvas()
    c.ring(24, 21, 18, 16, "steel", 3)
    for k in range(3):
        a = k * 2 * math.pi / 3 - 0.3
        pts = []
        for t in range(0, 11):
            ang = a + t * 0.09
            r = 3 + t * 1.25
            pts.append((24 + math.cos(ang) * r, 21 + math.sin(ang) * r))
        for t in range(10, -1, -1):
            ang = a + 0.75 + t * 0.03
            r = 3 + t * 1.2
            pts.append((24 + math.cos(ang) * r, 21 + math.sin(ang) * r))
        c.poly(pts, "ice", 3 + (k == 0))
    c.disc(24, 21, 3, "steel", 4)
    c.rect(22, 39, 25, 41, "steel", 2, bevel=False)
    c.rect(14, 42, 33, 44, "steel", 3)
    return fin(c)

def aircon():
    c = Canvas()
    c.rect(2, 8, 45, 26, "white", 4, radius=3)
    c.rect(4, 22, 43, 24, "white", 2, bevel=False)
    c.line(4, 22, 43, 22, ramp("graphite", 2))
    c.disc(40, 13, 1.4, "green", 4, rim=False)
    c.rect(6, 12, 20, 13, "white", 2, bevel=False)
    for i, x in enumerate((10, 18, 26, 34)):
        for k in range(12):
            c.set(x + round(math.sin(k * 0.9) * 1.4), 30 + k, ramp("ice", 4 if k < 7 else 3))
    return fin(c)

def scene():
    c = Canvas()
    # clapper/star "scena"
    pts = []
    for i in range(10):
        a = -math.pi / 2 + i * math.pi / 5
        r = 20 if i % 2 == 0 else 8.5
        pts.append((24 + math.cos(a) * r, 25 + math.sin(a) * r))
    c.poly(pts, "glass", 4)
    pts2 = [(24 + (p[0]-24)*0.55, 25 + (p[1]-25)*0.55 - 1) for p in pts]
    c.poly(pts2, "glass", 5)
    c.set(21, 18, WHITE)
    return fin(c)

def person():
    c = Canvas()
    c.disc(24, 14, 8, "cream", 4)
    c.rect(10, 26, 37, 44, "teal", 3, radius=6)
    c.rect(19, 26, 28, 30, "cream", 3, bevel=False)
    return fin(c)


def start():
    c = Canvas()
    c.rect(4, 6, 43, 41, "graphite", 3, radius=4)
    c.rect(8, 10, 22, 22, "amber", 3, radius=2)
    c.rect(25, 10, 39, 22, "teal", 3, radius=2)
    c.rect(8, 25, 22, 37, "screen", 3, radius=2)
    c.rect(25, 25, 39, 37, "red", 3, radius=2)
    for (x, y) in ((10, 12), (27, 12), (10, 27), (27, 27)):
        c.set(x, y, WHITE)
    return fin(c)


# ============================== odczyty czujników (24) ==============================

def sensor_temp():
    """Termometr: szklana rurka, czerwony słupek i bańka."""
    c = Canvas(24)
    c.rect(9, 2, 14, 15, "white", 4, radius=2)
    c.rect(11, 7, 12, 16, "red", 3, bevel=False)
    c.disc(11.5, 18, 4.6, "red", 3)
    c.set(10, 16, ramp("red", 5))
    for y in (5, 8, 11):
        c.line(15, y, 16, y, ramp("steel", 4))
    return fin(c)


def sensor_humidity():
    """Kropla wody z odblaskiem."""
    c = Canvas(24)
    c.poly([(12, 2), (17.5, 11), (18.5, 15), (12, 20), (5.5, 15), (6.5, 11)], "ice", 3)
    c.disc(12, 14.5, 6.4, "ice", 3)
    c.set(9, 12, WHITE)
    c.set(9, 13, WHITE)
    c.set(10, 11, ramp("ice", 5))
    return fin(c)


DEVICES = dict(radiator=radiator, thermostat=thermostat, aircon=aircon, blinds=blinds, plug=plug,
               sensor=sensor, vacuum=vacuum, lock=lock, fan=fan, person=person, scene=scene, start=start)
SENSORS = dict(temp=sensor_temp, humidity=sensor_humidity)


def preview(images: dict[str, Image.Image], path: pathlib.Path) -> None:
    cell = 104
    sheet = Image.new("RGBA", (cell * len(images), cell), (20, 22, 28, 255))
    for i, image in enumerate(images.values()):
        k = 96 // image.width
        sheet.alpha_composite(image.resize((image.width * k, image.height * k), Image.NEAREST), (i * cell + 4, 4))
    sheet.save(path)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    images = {f"dev_{name}": draw().image() for name, draw in DEVICES.items()}
    images.update({f"sensor_{name}": draw().image() for name, draw in SENSORS.items()})
    for name, image in images.items():
        image.save(OUT / f"{name}.png")
        if MOCKUP_DOM.is_dir():
            shutil.copyfile(OUT / f"{name}.png", MOCKUP_DOM / f"{name}.png")
        print(f"  {name}.png  {image.width}x{image.height}")
    if "--preview" in sys.argv:
        preview(images, pathlib.Path(sys.argv[sys.argv.index("--preview") + 1]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
