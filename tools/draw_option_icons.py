#!/usr/bin/env python3
"""Rysuje ikony opcji w pixel arcie: efekty lampy i tryby klimatyzacji (siatka 24 x 24).

Zastępują znaki Lucide (font_effect_icon, font_mode_icon) tym samym stylem co ikony zakładek
i pomieszczeń: kształty wypełnione rampą jednego koloru, światło z lewej góry, ciemny kontur
i cień. Mały kafel bierze obraz w 1 x (24 px), duży kafel bieżącej opcji w 2 x (48 px).

* efekty — `fx_<id>.png`: brak, przytulnie, relaks, seans TV, świeca, ciepła biel, złota biel,
  dzienne, zimna biel, skupienie, nocne, do snu, pobudka, puls, steampunk, diwali, alarm,
  plus `fx_more` na kafel „Więcej”,
* tryby klimy — `mode_<id>.png`: auto, cool, heat, fan_only, dry.

Wynik: docs/ux/mockup-pulpit/assets/pixel/ (źródło dla tools/gen_assets.py).

Uruchomienie:  tools/.venv/bin/python tools/draw_option_icons.py [--preview plik.png]
"""

from __future__ import annotations

import base64
import json
import math
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from draw_icons import OUTLINE, RAMPS, WHITE, Canvas, ramp  # noqa: E402

from PIL import Image, ImageDraw  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "ux" / "mockup-pulpit" / "assets" / "pixel"
MOCKUP = ROOT / "docs" / "ux" / "mockup-option-icons" / "index.html"
S = 24

RAMPS.setdefault("teal", ["#0c2f33", "#13504f", "#1d7a73", "#2fa39a", "#62cfc0", "#c8fff4"])
RAMPS.setdefault("blue", ["#14203f", "#1f3466", "#2d4f96", "#4570c4", "#77a0e6", "#d0e0ff"])
RAMPS.setdefault("white", ["#5a6274", "#8790a6", "#b4bccd", "#d7dce8", "#eef1f7", "#ffffff"])
RAMPS["ice"] = ["#123a5c", "#1d5f8f", "#2f8cc4", "#5bb6e8", "#a6dcf7", "#eefaff"]
RAMPS["brass"] = ["#3f2a0a", "#6e4b12", "#a0701c", "#c99a2e", "#e9c35a", "#fff0b0"]
RAMPS["clay"] = ["#3a1a10", "#62301c", "#8c4a2a", "#b0653c", "#d48a5c", "#f0b98c"]


def fin(c: Canvas) -> Canvas:
    c.outline()
    c.drop_shadow()
    return c


def rays(c, cx, cy, r0, r1, n, name, level, offset=0.0):
    for i in range(n):
        a = offset + i * 2 * math.pi / n
        c.line(cx + math.cos(a) * r0, cy + math.sin(a) * r0, cx + math.cos(a) * r1, cy + math.sin(a) * r1,
               ramp(name, level))


def flame(c, cx, top, h, big=True):
    """Płomień: czerwony brzeg, bursztyn, jasny środek."""
    w = 3.6 if big else 2.6
    c.poly([(cx, top), (cx + w, top + h * 0.6), (cx + w * 0.7, top + h), (cx - w * 0.7, top + h),
            (cx - w, top + h * 0.6)], "red", 3)
    c.poly([(cx, top + h * 0.25), (cx + w * 0.62, top + h * 0.65), (cx + w * 0.45, top + h),
            (cx - w * 0.45, top + h), (cx - w * 0.62, top + h * 0.65)], "amber", 4)
    c.poly([(cx, top + h * 0.55), (cx + 1.2, top + h * 0.8), (cx + 0.8, top + h), (cx - 0.8, top + h),
            (cx - 1.2, top + h * 0.8)], "glass", 5)


def bulb(c, glass, level):
    c.disc(12, 9, 6.5, glass, level, rim=False)
    c.set(9, 6, WHITE)
    c.set(10, 5, WHITE)
    c.rect(10, 15, 14, 16, glass, level - 1, bevel=False)
    c.rect(9, 17, 15, 18, "steel", 4, bevel=False)
    c.rect(9, 19, 15, 19, "steel", 3, bevel=False)
    c.rect(10, 20, 14, 20, "graphite", 2, bevel=False)


# ============================== efekty ==============================

def fx_none():
    """Brak: przekreślone kółko."""
    c = Canvas(S)
    c.ring(12, 11.5, 9, 6.6, "steel", 3)
    for i in range(-6, 7):
        c.set(12 + i, 11 - i, ramp("red", 3))
        c.set(12 + i + 1, 11 - i, ramp("red", 2))
    return fin(c)


def fx_cozy():
    """Przytulnie: kubek kakao z parą."""
    c = Canvas(S)
    for x, ph in ((8, 0), (12, 1), (16, 0)):
        for y in range(2, 8):
            c.set(x + (1 if (y + ph) % 4 < 2 else 0), y, ramp("white", 3 + (y < 4)))
    c.rect(4, 9, 16, 20, "red", 3, radius=1)
    c.rect(5, 9, 15, 10, "wood", 2, bevel=False)
    c.ring(17.5, 14.5, 4, 2, "red", 3)
    c.rect(5, 13, 15, 14, "cream", 4, bevel=False)
    return fin(c)


def fx_relax():
    """Relaks: liść."""
    c = Canvas(S)
    c.poly([(4, 19), (6, 9), (12, 4), (20, 3), (19, 11), (14, 17), (7, 19)], "green", 3)
    c.poly([(6, 10), (12, 5), (16, 4), (11, 8)], "green", 4)
    c.line(5, 20, 17, 6, ramp("green", 1))
    for i in range(3):
        c.line(9 + i * 3, 16 - i * 3, 9 + i * 3, 12 - i * 3, ramp("green", 2))
    return fin(c)


def fx_tv():
    """Seans TV: mały kineskop z anteną."""
    c = Canvas(S)
    c.line(11, 6, 7, 1, ramp("steel", 4))
    c.line(12, 6, 16, 1, ramp("steel", 3))
    c.rect(2, 6, 21, 19, "wood", 3, radius=1)
    c.vgrad(4, 8, 16, 17, "screen", 4.6, 2.2, radius=1)
    c.set(5, 9, WHITE)
    c.disc(18.5, 10, 1.3, "amber", 4, rim=False)
    c.disc(18.5, 14, 1.3, "steel", 3, rim=False)
    c.rect(5, 20, 6, 21, "graphite", 2, bevel=False)
    c.rect(17, 20, 18, 21, "graphite", 2, bevel=False)
    return fin(c)


def fx_candle():
    """Świeca: wosk, knot, płomień."""
    c = Canvas(S)
    flame(c, 12, 0, 9)
    c.set(12, 9, ramp("graphite", 1))
    c.rect(8, 11, 15, 20, "cream", 4, radius=1)
    c.rect(9, 11, 10, 14, "cream", 5, bevel=False)
    c.rect(6, 20, 17, 21, "brass", 3)
    return fin(c)


def fx_warm():
    """Ciepła biel: żarówka w ciepłym bursztynie."""
    c = Canvas(S)
    bulb(c, "amber", 4)
    return fin(c)


def fx_golden():
    """Złota biel: złote słońce z krótkimi promieniami."""
    c = Canvas(S)
    rays(c, 12, 11.5, 7.5, 10, 8, "brass", 4, math.pi / 8)
    c.disc(12, 11.5, 6, "brass", 4)
    c.set(9, 8, WHITE)
    return fin(c)


def fx_daylight():
    """Dzienne: jasne słońce z długimi promieniami."""
    c = Canvas(S)
    rays(c, 12, 11.5, 6.5, 10.5, 8, "glass", 5)
    c.disc(12, 11.5, 5, "glass", 4.4)
    c.set(10, 9, WHITE)
    c.set(11, 9, WHITE)
    return fin(c)


def fx_cool():
    """Zimna biel: żarówka w chłodnym błękicie."""
    c = Canvas(S)
    bulb(c, "ice", 4)
    return fin(c)


def fx_focus():
    """Skupienie: tarcza strzelecka."""
    c = Canvas(S)
    c.ring(12, 11.5, 9.5, 7, "red", 3)
    c.ring(12, 11.5, 7, 4.5, "white", 4)
    c.disc(12, 11.5, 4.5, "red", 3, light=False, rim=False)
    c.disc(12, 11.5, 2, "white", 5, light=False, rim=False)
    return fin(c)


def fx_night():
    """Nocne: sierp księżyca i gwiazda."""
    c = Canvas(S)
    c.disc(10, 12, 8, "cream", 4)
    for y in range(S):
        for x in range(S):
            if math.hypot(x + 0.5 - 14.5, y + 0.5 - 8.5) < 7:
                c.px[y][x] = None
    for dx, dy in ((0, 0), (1, 0), (-1, 0), (0, 1), (0, -1)):
        c.set(19 + dx, 15 + dy, ramp("glass", 5 if (dx, dy) == (0, 0) else 4))
    c.set(17, 4, ramp("glass", 5))
    return fin(c)


def fx_bedtime():
    """Do snu: poduszka z kołdrą i „z”."""
    c = Canvas(S)
    for i, (x, y) in enumerate(((14, 2), (18, 5))):
        n = 3 if i == 0 else 2
        c.line(x, y, x + n, y, ramp("white", 4))
        c.line(x + n, y, x, y + n, ramp("white", 4))
        c.line(x, y + n, x + n, y + n, ramp("white", 4))
    c.rect(2, 13, 9, 17, "white", 4, radius=1)
    c.rect(2, 15, 21, 20, "violet", 3, radius=1)
    c.line(3, 15, 20, 15, ramp("violet", 5))
    c.rect(1, 9, 2, 21, "wood", 3, bevel=False)
    c.rect(21, 14, 22, 21, "wood", 3, bevel=False)
    return fin(c)


def fx_wakeup():
    """Pobudka: wschodzące słońce nad horyzontem."""
    c = Canvas(S)
    rays(c, 12, 15, 7, 10, 7, "amber", 4, math.pi)
    c.disc(12, 15, 5.8, "amber", 4)
    for y in range(15, S):
        for x in range(S):
            c.px[y][x] = None
    c.rect(1, 15, 22, 16, "red", 3, bevel=False)
    c.rect(4, 18, 19, 18, "red", 2, bevel=False)
    c.rect(8, 20, 15, 20, "red", 2, bevel=False)
    return fin(c)


def fx_pulse():
    """Puls: serce z linią EKG."""
    c = Canvas(S)
    c.disc(8, 8.5, 4.5, "red", 3, rim=False)
    c.disc(16, 8.5, 4.5, "red", 3, rim=False)
    c.poly([(3.6, 10), (20.4, 10), (12, 19.5)], "red", 3)
    pts = [(1, 12), (7, 12), (9, 8), (11, 16), (13, 10), (15, 12), (22, 12)]
    for (ax, ay), (bx, by) in zip(pts, pts[1:]):
        c.line(ax, ay, bx, by, WHITE)
    return fin(c)


def fx_steampunk():
    """Steampunk: mosiężne koło zębate."""
    c = Canvas(S)
    for i in range(8):
        a = i * math.pi / 4
        cx, cy = 12 + math.cos(a) * 8, 11.5 + math.sin(a) * 8
        c.rect(int(cx - 1), int(cy - 1), int(cx + 1), int(cy + 1), "brass", 3, bevel=False)
    c.ring(12, 11.5, 8, 3, "brass", 3)
    c.ring(12, 11.5, 5, 3.6, "brass", 2)
    return fin(c)


def fx_diwali():
    """Diwali: gliniana lampka diya z płomieniem."""
    c = Canvas(S)
    flame(c, 12, 1, 10)
    c.poly([(2, 12), (22, 12), (18, 18), (6, 18)], "clay", 3)
    c.line(3, 12, 21, 12, ramp("clay", 5))
    c.rect(8, 18, 15, 19, "clay", 2, bevel=False)
    for x in (6, 10, 14, 18):
        c.set(x, 14, ramp("brass", 5))
    return fin(c)


def fx_alarm():
    """Alarm: budzik z dzwonkami."""
    c = Canvas(S)
    c.disc(5, 4.5, 2.6, "brass", 4, rim=False)
    c.disc(19, 4.5, 2.6, "brass", 4, rim=False)
    c.disc(12, 12.5, 8, "red", 3)
    c.disc(12, 12.5, 5.8, "white", 5, light=False, rim=False)
    c.line(12, 12, 12, 8, ramp("graphite", 1))
    c.line(12, 12, 15, 13, ramp("graphite", 1))
    c.line(6, 19, 4, 21, ramp("graphite", 2))
    c.line(18, 19, 20, 21, ramp("graphite", 2))
    return fin(c)


def fx_more():
    """Więcej: siatka 2 x 2."""
    c = Canvas(S)
    for x, y in ((3, 3), (13, 3), (3, 13), (13, 13)):
        c.rect(x, y, x + 7, y + 7, "steel", 3, radius=1)
    return fin(c)


# ============================== tryby klimy ==============================

def mode_auto():
    """Auto: iskry (jak w Lucide), w bursztynie."""
    c = Canvas(S)

    def sparkle(cx, cy, r, level):
        c.poly([(cx, cy - r), (cx + r * 0.28, cy - r * 0.28), (cx + r, cy), (cx + r * 0.28, cy + r * 0.28),
                (cx, cy + r), (cx - r * 0.28, cy + r * 0.28), (cx - r, cy), (cx - r * 0.28, cy - r * 0.28)],
               "glass", level)

    sparkle(10, 13, 8.5, 4)
    sparkle(18.5, 4.5, 3.5, 5)
    sparkle(19, 18.5, 2.6, 4)
    c.set(9, 11, WHITE)
    return fin(c)


def mode_cool():
    """Chłodzenie: płatek śniegu."""
    c = Canvas(S)
    for i in range(3):
        a = i * math.pi / 3
        dx, dy = math.cos(a) * 9.5, math.sin(a) * 9.5
        c.line(12 - dx, 11.5 - dy, 12 + dx, 11.5 + dy, ramp("ice", 4))
        c.line(12 - dx + 1, 11.5 - dy, 12 + dx + 1, 11.5 + dy, ramp("ice", 3))
        c.line(12 - dx, 11.5 - dy + 1, 12 + dx, 11.5 + dy + 1, ramp("ice", 3))
        for s in (-1, 1):
            ex, ey = 12 + s * math.cos(a) * 6.5, 11.5 + s * math.sin(a) * 6.5
            for b in (a + 2.4, a - 2.4):
                b2 = b if s > 0 else b + math.pi
                c.line(ex, ey, ex + math.cos(b2) * 3.4, ey + math.sin(b2) * 3.4, ramp("ice", 4))
    c.disc(12, 11.5, 2.2, "white", 5, light=False, rim=False)
    return fin(c)


def mode_heat():
    """Grzanie: duży płomień."""
    c = Canvas(S)
    c.poly([(12, 1), (17, 8), (19, 14), (16, 20), (8, 20), (5, 14), (7, 8), (9, 11)], "red", 3)
    c.poly([(12, 7), (15, 12), (15, 17), (12, 20), (9, 17), (9.5, 13)], "amber", 4)
    c.poly([(12, 12), (13.6, 15.5), (12, 19), (10.4, 15.5)], "glass", 5)
    return fin(c)


def mode_fan():
    """Wentylator: trzy płaskie łopatki wokół osi."""
    c = Canvas(S)
    for i in range(3):
        a = i * 2 * math.pi / 3 - math.pi / 2
        pts = [(12 + math.cos(a + d) * r, 11.5 + math.sin(a + d) * r)
               for d, r in ((-0.55, 2.5), (-0.25, 7.5), (0.0, 9.6), (0.35, 9.8), (0.7, 7.5), (0.9, 3))]
        c.poly(pts, "steel", 4 if i == 0 else 3)
    c.disc(12, 11.5, 2.6, "graphite", 3)
    c.set(11, 10, ramp("steel", 5))
    return fin(c)


def mode_dry():
    """Osuszanie: dwie krople."""
    c = Canvas(S)

    def drop(cx, top, h, level):
        r = h * 0.36
        c.disc(cx, top + h - r, r, "blue", level)
        c.poly([(cx, top), (cx + r * 0.95, top + h - r), (cx - r * 0.95, top + h - r)], "blue", level)

    drop(9, 3, 15, 3)
    drop(17, 9, 10, 4)
    c.set(7, 12, WHITE)
    c.set(16, 15, WHITE)
    return fin(c)


EFFECTS = {
    "none": fx_none, "cozy": fx_cozy, "relax": fx_relax, "tv": fx_tv, "candle": fx_candle,
    "warm": fx_warm, "golden": fx_golden, "daylight": fx_daylight, "cool": fx_cool, "focus": fx_focus,
    "night": fx_night, "bedtime": fx_bedtime, "wakeup": fx_wakeup, "pulse": fx_pulse,
    "steampunk": fx_steampunk, "diwali": fx_diwali, "alarm": fx_alarm, "more": fx_more,
}
MODES = {"auto": mode_auto, "cool": mode_cool, "heat": mode_heat, "fan_only": mode_fan, "dry": mode_dry}

LABELS = {
    "none": "Brak", "cozy": "Przytulnie", "relax": "Relaks", "tv": "Seans TV", "candle": "Świeca",
    "warm": "Ciepła biel", "golden": "Złota biel", "daylight": "Dzienne", "cool": "Zimna biel",
    "focus": "Skupienie", "night": "Nocne", "bedtime": "Do snu", "wakeup": "Pobudka", "pulse": "Puls",
    "steampunk": "Steampunk", "diwali": "Diwali", "alarm": "Alarm", "more": "Więcej",
    "auto": "Auto", "heat": "Grzanie", "fan_only": "Wentylator", "dry": "Osuszanie",
}


def images() -> dict[str, Image.Image]:
    out = {f"fx_{k}": f().image() for k, f in EFFECTS.items()}
    out.update({f"mode_{k}": f().image() for k, f in MODES.items()})
    return out


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    imgs = images()
    for name, image in imgs.items():
        image.save(OUT / f"{name}.png")
    print(f"  {len(imgs)} ikon -> {OUT.relative_to(ROOT)}")
    if MOCKUP.exists():
        # Makieta jako jeden plik: obrazy w data URI między znacznikami.
        data = {name: "data:image/png;base64," + base64.b64encode((OUT / f"{name}.png").read_bytes()).decode()
                for name in imgs}
        html = re.sub(r"/\*ICONS\*/.*?/\*/ICONS\*/", lambda _: "/*ICONS*/" + json.dumps(data) + "/*/ICONS*/",
                      MOCKUP.read_text(), flags=re.S)
        MOCKUP.write_text(html)
        print(f"  makieta -> {MOCKUP.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
