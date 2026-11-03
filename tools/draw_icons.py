#!/usr/bin/env python3
"""Rysuje ikony zakładek Pulpitu w pixel arcie na siatce 48 x 48.

Każda ikona składa się z prostych kształtów wypełnianych rampą jednego koloru
(światło z lewej góry, cień z prawej dołu), a na końcu dostaje ciemny kontur
i cień pod spodem — jak w zestawach, na których wzorowany jest styl.

Wynik to PNG 48 x 48 z przezroczystym tłem w docs/ux/mockup-pulpit/assets/pixel/.
Te pliki są źródłem prawdy: makieta pokazuje je wprost, a tools/gen_assets.py
robi z nich obrazy LVGL (48 px na zakładkę, 96 px na kafelek startowy).
Poprawia się je tutaj albo w dowolnym edytorze pikseli.

Uruchomienie:  tools/.venv/bin/python tools/draw_icons.py [--preview plik.png]
"""

from __future__ import annotations

import math
import pathlib
import sys

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "ux" / "mockup-pulpit" / "assets" / "pixel"
SIZE = 48

# ---------- paleta: rampy od najciemniejszego do najjaśniejszego ----------
OUTLINE = (14, 16, 24, 255)
SHADOW = (0, 0, 0, 90)

RAMPS = {
    "steel": ["#1e2433", "#2f3a52", "#46557a", "#6a7ca3", "#9fb1d3", "#dfe7f5"],
    "graphite": ["#15171d", "#22262f", "#323844", "#474f5e", "#69738a", "#a3acc0"],
    "screen": ["#0b2a4a", "#124a7a", "#1f6fb0", "#3a98d8", "#77c4f0", "#d8f2ff"],
    "amber": ["#6b3a0c", "#a85f15", "#d98a22", "#f2a93b", "#ffd27a", "#fff2c8"],
    "red": ["#4a1018", "#7d1c26", "#b52d34", "#e04a46", "#ff7e6b", "#ffc6b8"],
    "wood": ["#3a2014", "#5c331d", "#83492a", "#a8653c", "#cc8a5a", "#ebb98c"],
    "cream": ["#6e6050", "#9b8a72", "#c4b294", "#e2d3b4", "#f5ebd4", "#fffaf0"],
    "green": ["#123a24", "#1d5c37", "#2b844c", "#43ad64", "#76d189", "#c3f2c4"],
    "violet": ["#24163f", "#3b2468", "#583896", "#7d56c2", "#a985e6", "#e1d2ff"],
    "glass": ["#2b1d08", "#6b4410", "#c4831e", "#f7b733", "#ffe27a", "#fffbe0"],
}


def hex_rgba(value: str) -> tuple[int, int, int, int]:
    return (int(value[1:3], 16), int(value[3:5], 16), int(value[5:7], 16), 255)


def ramp(name: str, level: float) -> tuple[int, int, int, int]:
    colors = RAMPS[name]
    index = max(0, min(len(colors) - 1, int(round(level))))
    return hex_rgba(colors[index])


class Canvas:
    def __init__(self, size: int = SIZE) -> None:
        self.size = size
        self.px: list[list[tuple[int, int, int, int] | None]] = [
            [None] * size for _ in range(size)
        ]

    def set(self, x: int, y: int, color) -> None:
        if 0 <= x < self.size and 0 <= y < self.size:
            self.px[y][x] = color

    def get(self, x: int, y: int):
        if 0 <= x < self.size and 0 <= y < self.size:
            return self.px[y][x]
        return None

    # --- kształty z oświetleniem ---
    def rect(self, x0, y0, x1, y1, name, base=3, bevel=True, radius=0):
        """Prostokąt [x0, x1] x [y0, y1] z fazką: jaśniej u góry i z lewej."""
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if radius and _corner_cut(x, y, x0, y0, x1, y1, radius):
                    continue
                level = base
                if bevel:
                    if y == y0 or x == x0:
                        level = base + 1
                    if y == y1 or x == x1:
                        level = base - 1
                    if (y == y0 and x == x0):
                        level = base + 2
                self.set(x, y, ramp(name, level))

    def vgrad(self, x0, y0, x1, y1, name, top, bottom, radius=0):
        """Pionowy gradient z rampy, schodkowo (bez mieszania kolorów)."""
        height = max(1, y1 - y0)
        for y in range(y0, y1 + 1):
            level = top + (bottom - top) * (y - y0) / height
            for x in range(x0, x1 + 1):
                if radius and _corner_cut(x, y, x0, y0, x1, y1, radius):
                    continue
                self.set(x, y, ramp(name, level))

    def disc(self, cx, cy, r, name, base=3, light=True, rim=True):
        """Koło oświetlone z lewej góry: kula, jeśli `light`, płaskie w przeciwnym razie."""
        for y in range(int(cy - r - 1), int(cy + r + 2)):
            for x in range(int(cx - r - 1), int(cx + r + 2)):
                dx, dy = x + 0.5 - cx, y + 0.5 - cy
                d = math.hypot(dx, dy)
                if d > r:
                    continue
                level = base
                if light:
                    # Iloczyn z kierunkiem światła (-1, -1): jaśniej po stronie lampy.
                    shade = (-dx - dy) / (r * 1.6)
                    level = base + shade * 2.2
                if rim and d > r - 1.0:
                    level = min(level, base - 1)
                self.set(x, y, ramp(name, level))

    def ring(self, cx, cy, r_out, r_in, name, base=3):
        for y in range(int(cy - r_out - 1), int(cy + r_out + 2)):
            for x in range(int(cx - r_out - 1), int(cx + r_out + 2)):
                dx, dy = x + 0.5 - cx, y + 0.5 - cy
                d = math.hypot(dx, dy)
                if r_in < d <= r_out:
                    shade = (-dx - dy) / (r_out * 1.6)
                    self.set(x, y, ramp(name, base + shade * 2.2))

    def line(self, x0, y0, x1, y1, color):
        steps = int(max(abs(x1 - x0), abs(y1 - y0)))
        for i in range(steps + 1):
            t = i / steps if steps else 0
            self.set(round(x0 + (x1 - x0) * t), round(y0 + (y1 - y0) * t), color)

    def poly(self, points, name, base=3):
        """Wielokąt wypukły lub nie (reguła parzystości), płaski kolor z rampy."""
        ys = [p[1] for p in points]
        for y in range(int(min(ys)), int(max(ys)) + 1):
            yc = y + 0.5
            xs = []
            for (ax, ay), (bx, by) in zip(points, points[1:] + points[:1]):
                if (ay <= yc < by) or (by <= yc < ay):
                    xs.append(ax + (yc - ay) * (bx - ax) / (by - ay))
            xs.sort()
            for a, b in zip(xs[0::2], xs[1::2]):
                for x in range(int(math.ceil(a - 0.5)), int(math.floor(b - 0.5)) + 1):
                    self.set(x, y, ramp(name, base))

    # --- wykończenie ---
    def outline(self):
        """Ciemny kontur wokół wszystkiego, co narysowano (sąsiedztwo czterech)."""
        add = []
        for y in range(self.size):
            for x in range(self.size):
                if self.px[y][x] is not None:
                    continue
                if any(self.get(x + dx, y + dy) not in (None, OUTLINE)
                       for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                    add.append((x, y))
        for x, y in add:
            self.px[y][x] = OUTLINE

    def drop_shadow(self):
        """Półprzezroczysty cień o piksel w dół i w prawo, pod konturem."""
        add = []
        for y in range(self.size):
            for x in range(self.size):
                if self.px[y][x] is None and self.get(x - 1, y - 1) is not None:
                    add.append((x, y))
        for x, y in add:
            self.px[y][x] = SHADOW

    def image(self) -> Image.Image:
        img = Image.new("RGBA", (self.size, self.size), (0, 0, 0, 0))
        for y in range(self.size):
            for x in range(self.size):
                if self.px[y][x] is not None:
                    img.putpixel((x, y), self.px[y][x])
        return img


def _corner_cut(x, y, x0, y0, x1, y1, r):
    for cx, cy in ((x0 + r, y0 + r), (x1 - r, y0 + r), (x0 + r, y1 - r), (x1 - r, y1 - r)):
        inside_x = (x < x0 + r and cx == x0 + r) or (x > x1 - r and cx == x1 - r)
        inside_y = (y < y0 + r and cy == y0 + r) or (y > y1 - r and cy == y1 - r)
        if inside_x and inside_y and math.hypot(x + 0.5 - (cx + 0.5 if cx == x1 - r else cx),
                                                 y + 0.5 - (cy + 0.5 if cy == y1 - r else cy)) > r + 0.2:
            return True
    return False


WHITE = (255, 255, 255, 255)


# ============================== ikony ==============================

def icon_tv() -> Canvas:
    """Telewizor kineskopowy z anteną, pokrętłami i kratką głośnika."""
    c = Canvas()
    # antena: dwa pręty z kulkami
    c.line(23, 13, 14, 3, ramp("steel", 4))
    c.line(24, 13, 33, 4, ramp("steel", 3))
    c.disc(14, 3, 1.6, "red", 3, rim=False)
    c.disc(33.5, 4, 1.6, "red", 3, rim=False)
    c.rect(19, 11, 28, 14, "graphite", 3)
    # obudowa
    c.rect(3, 14, 44, 41, "wood", 3, radius=3)
    c.rect(5, 39, 42, 41, "wood", 1, bevel=False)  # cień spodu obudowy
    # ekran: ramka, szkło z gradientem, odblaski
    c.rect(6, 17, 32, 37, "graphite", 1, radius=3)
    c.vgrad(8, 19, 30, 35, "screen", 4.6, 1.2, radius=3)
    for i in range(5):
        c.set(11 + i, 21 + i // 2, ramp("screen", 5))  # długi refleks
    c.line(10, 22, 13, 21, ramp("screen", 5))
    c.line(10, 24, 11, 23, ramp("screen", 5))
    c.set(28, 33, ramp("screen", 4))
    # obrazek na ekranie: słońce i dwa pagórki
    c.disc(25, 24, 2.4, "amber", 4, rim=False)
    for x in range(8, 31):
        hill_a = 31 - int(3.2 * math.sin((x - 6) / 7.5))
        hill_b = 33 - int(2.2 * math.sin((x + 4) / 5.0))
        for y in range(min(hill_a, hill_b), 36):
            if c.get(x, y) is None or c.get(x, y)[2] > c.get(x, y)[1]:
                level = 3.4 if y < hill_b else 2.2
                if y in (hill_a, hill_b):
                    level += 1
                c.set(x, y, ramp("green", level))
    # linie kineskopu: co drugi rząd ciut ciemniej
    for y in range(20, 35, 2):
        for x in range(9, 30):
            if c.get(x, y) == ramp("screen", 2):
                c.set(x, y, ramp("screen", 1))
    # panel boczny: pokrętła i kratka
    c.rect(34, 17, 41, 37, "wood", 2, bevel=False)
    c.disc(37.5, 21, 2.6, "amber", 3)
    c.set(37, 19, ramp("amber", 5))
    c.disc(37.5, 27.5, 2.6, "steel", 3)
    c.set(37, 26, ramp("steel", 5))
    for y in range(32, 37, 2):
        c.line(35, y, 40, y, ramp("wood", 0))
    # nóżki
    c.rect(8, 42, 12, 44, "graphite", 2)
    c.rect(35, 42, 39, 44, "graphite", 2)
    c.outline()
    c.drop_shadow()
    return c


def icon_appletv() -> Canvas:
    """Płaski dekoder z błyszczącym blatem i pilotem Siri obok."""
    c = Canvas()
    # dekoder: blat (jaśniejszy trapez) i front
    c.poly([(2, 20), (33, 20), (40, 13), (9, 13)], "graphite", 4)
    c.line(9, 13, 39, 13, ramp("graphite", 5))
    c.rect(2, 20, 33, 35, "graphite", 2, radius=1)
    c.poly([(33, 20), (40, 13), (40, 28), (33, 35)], "graphite", 1)
    # połysk blatu
    c.line(12, 15, 24, 15, ramp("graphite", 5))
    c.line(11, 16, 17, 16, ramp("graphite", 5))
    c.set(30, 18, ramp("graphite", 5))
    # znak odtwarzania na froncie, w okrągłym wgłębieniu
    c.disc(17.5, 27.5, 5.2, "graphite", 1.2, light=False, rim=False)
    c.poly([(15, 23.5), (15, 31.5), (22, 27.5)], "steel", 5)
    c.set(15, 24, WHITE)
    # dioda
    c.set(30, 33, ramp("amber", 5))
    c.set(29, 33, ramp("amber", 3))
    # nóżka-cień
    c.rect(5, 36, 30, 37, "graphite", 0, bevel=False)
    # pilot Siri: smukły, szklany panel dotykowy u góry
    c.rect(37, 22, 45, 46, "steel", 3, radius=2)
    c.rect(38, 23, 44, 31, "graphite", 1, radius=1)
    c.disc(41, 27, 2.2, "graphite", 2, light=False, rim=False)
    c.set(39, 24, ramp("graphite", 4))
    c.disc(41, 35, 1.5, "graphite", 2, rim=False)
    c.disc(41, 40, 1.5, "graphite", 2, rim=False)
    c.outline()
    c.drop_shadow()
    return c


def icon_audio() -> Canvas:
    """Kolumna głośnikowa: drewno, wysokotonowy, niskotonowy z membraną, dioda."""
    c = Canvas()
    c.rect(11, 2, 36, 44, "graphite", 2, radius=2)
    c.rect(13, 4, 34, 42, "graphite", 3, bevel=False, radius=1)
    # wysokotonowy
    c.ring(24, 12.5, 5.5, 4.2, "steel", 3)
    c.disc(24, 12.5, 4.2, "graphite", 1, light=False, rim=False)
    c.disc(24, 12.5, 2.4, "steel", 4, rim=False)
    c.set(23, 11, WHITE)
    # niskotonowy: kosz, zawieszenie, membrana, nakładka
    c.ring(24, 30, 10, 8.6, "steel", 3)
    c.ring(24, 30, 8.6, 7.4, "graphite", 1)
    c.disc(24, 30, 7.4, "graphite", 2.4)
    for r in (5.6, 3.8):
        c.ring(24, 30, r + 0.5, r - 0.3, "graphite", 1.4)
    c.disc(24, 30, 2.6, "steel", 4, rim=False)
    c.set(23, 29, WHITE)
    # śruby w rogach
    for sx, sy in ((14, 5), (33, 5), (14, 41), (33, 41)):
        c.set(sx, sy, ramp("steel", 4))
    # dioda zasilania
    c.set(24, 21, ramp("amber", 5))
    c.set(23, 21, ramp("amber", 3))
    c.set(25, 21, ramp("amber", 3))
    c.outline()
    c.drop_shadow()
    return c


def icon_home() -> Canvas:
    """Dom z dachówką, kominem, oświetlonymi oknami, drzwiami i trawą."""
    c = Canvas()
    # komin z dymkiem
    c.rect(31, 6, 35, 16, "red", 2)
    c.rect(30, 5, 36, 6, "graphite", 3, bevel=False)
    c.disc(35, 3, 1.6, "steel", 4.2, rim=False)
    c.disc(38.5, 1.5, 1.2, "steel", 4.6, rim=False)
    # ściany
    c.rect(8, 22, 39, 42, "cream", 3)
    # dach: dachówki w rzędach
    roof = [(3, 24), (24, 5), (45, 24)]
    c.poly(roof, "red", 3)
    for y in range(9, 24, 3):
        for x in range(0, SIZE):
            if c.get(x, y) == ramp("red", 3):
                c.set(x, y, ramp("red", 2))
        for x in range(y % 6, SIZE, 6):
            if c.get(x, y + 1) == ramp("red", 3):
                c.set(x, y + 1, ramp("red", 2))
    c.line(4, 24, 24, 6, ramp("red", 5))  # światło na krawędzi dachu
    c.line(25, 6, 44, 24, ramp("red", 1))
    c.rect(4, 24, 43, 25, "graphite", 2, bevel=False)  # okap
    # okna: rama, ciepłe światło, krzyż, parapet
    for wx in (11, 29):
        c.rect(wx, 28, wx + 7, 35, "wood", 2)
        c.vgrad(wx + 1, 29, wx + 6, 34, "glass", 5, 3)
        c.line(wx + 1, 31, wx + 6, 31, ramp("wood", 2))
        c.line(wx + 3, 29, wx + 3, 34, ramp("wood", 2))
        c.set(wx + 1, 29, WHITE)
        c.rect(wx - 1, 36, wx + 8, 36, "cream", 1, bevel=False)
    # drzwi z klamką
    c.rect(20, 30, 27, 42, "wood", 3, radius=1)
    c.line(21, 33, 26, 33, ramp("wood", 2))
    c.line(21, 37, 26, 37, ramp("wood", 2))
    c.set(26, 36, ramp("amber", 5))
    # trawa
    c.rect(4, 43, 43, 45, "green", 3, bevel=False)
    for x in range(5, 43, 3):
        c.set(x, 42, ramp("green", 4))
    c.line(4, 43, 43, 43, ramp("green", 4))
    c.outline()
    c.drop_shadow()
    return c


def icon_settings() -> Canvas:
    """Metalowe koło zębate z otworem i mniejszym, bursztynowym kołem obok."""
    c = Canvas()

    def gear(cx, cy, r_body, r_tooth, teeth, hole, name):
        for y in range(SIZE):
            for x in range(SIZE):
                dx, dy = x + 0.5 - cx, y + 0.5 - cy
                d = math.hypot(dx, dy)
                angle = math.atan2(dy, dx)
                phase = (angle / (2 * math.pi) * teeth) % 1.0
                radius = r_tooth if 0.2 < phase < 0.7 else r_body
                if d <= radius and d > hole:
                    shade = (-dx - dy) / (r_tooth * 1.5)
                    level = 3 + shade * 2.2
                    if d > radius - 1:
                        level = min(level, 2)
                    if d < hole + 1.2:
                        level = 1
                    c.set(x, y, ramp(name, level))

    gear(19, 21, 12, 15.5, 9, 4.5, "steel")
    gear(36, 36, 6.5, 9, 7, 2.5, "amber")
    # wewnętrzny pierścień na dużym kole
    for y in range(SIZE):
        for x in range(SIZE):
            d = math.hypot(x + 0.5 - 19, y + 0.5 - 21)
            if 8.4 < d <= 9.3 and c.get(x, y) is not None:
                c.set(x, y, ramp("steel", 2))
    c.set(14, 15, WHITE)
    c.set(15, 14, WHITE)
    c.outline()
    c.drop_shadow()
    return c


# ============================== pogoda (32 x 32) ==============================
# Ekran startowy pokazuje je w skali 2 x (64 px), obok zegara (ADR 0011).
WEATHER_SIZE = 32


def _sun(c, cx, cy, r, rays=True):
    if rays:
        for i in range(8):
            angle = i * math.pi / 4
            x0, y0 = cx + math.cos(angle) * (r + 2), cy + math.sin(angle) * (r + 2)
            x1, y1 = cx + math.cos(angle) * (r + 4.5), cy + math.sin(angle) * (r + 4.5)
            c.line(x0, y0, x1, y1, ramp("amber", 4 if i in (5, 6, 7, 4) else 3))
    c.disc(cx, cy, r, "amber", 3.4)
    c.set(int(cx - r / 2), int(cy - r / 2), ramp("amber", 5))


def _cloud(c, x, y, w, name="steel", base=4):
    """Chmura z trzech kul i płaskiego spodu; `x, y` to lewy dolny róg."""
    h = w * 0.5
    c.disc(x + w * 0.30, y - h * 0.45, w * 0.22, name, base)
    c.disc(x + w * 0.58, y - h * 0.62, w * 0.28, name, base + 0.3)
    c.disc(x + w * 0.80, y - h * 0.38, w * 0.19, name, base - 0.2)
    c.rect(int(x + w * 0.12), int(y - h * 0.45), int(x + w * 0.95), int(y), name, base - 0.5,
           bevel=False, radius=2)
    c.line(int(x + w * 0.15), int(y), int(x + w * 0.92), int(y), ramp(name, base - 1.5))


def _drops(c, xs, y0, name="screen", length=3):
    for i, x in enumerate(xs):
        top = y0 + (i % 2) * 2
        c.line(x, top, x - 1, top + length, ramp(name, 4))
        c.set(x, top, ramp(name, 5))


def weather_sunny():
    c = Canvas(WEATHER_SIZE)
    _sun(c, 16, 16, 7)
    c.outline()
    c.drop_shadow()
    return c


def weather_night():
    c = Canvas(WEATHER_SIZE)
    c.disc(15, 16, 10, "cream", 4)
    # Wycięcie sierpa: druga, przesunięta kula zdejmuje część księżyca.
    for y in range(WEATHER_SIZE):
        for x in range(WEATHER_SIZE):
            if math.hypot(x + 0.5 - 21, y + 0.5 - 12) < 8.5:
                c.set(x, y, None)
    c.set(9, 12, ramp("cream", 5))
    for x, y in ((24, 22), (27, 8), (22, 4)):
        c.set(x, y, ramp("glass", 5))
    c.outline()
    c.drop_shadow()
    return c


def weather_partly():
    c = Canvas(WEATHER_SIZE)
    _sun(c, 11, 11, 5.5)
    _cloud(c, 6, 27, 24, "steel", 4.4)
    c.outline()
    c.drop_shadow()
    return c


def weather_partly_night():
    """`partlycloudy` w nocy: sierp księżyca za chmurą zamiast słońca."""
    c = Canvas(WEATHER_SIZE)
    c.disc(11, 11, 7, "cream", 4)
    for y in range(WEATHER_SIZE):
        for x in range(WEATHER_SIZE):
            if math.hypot(x + 0.5 - 15.5, y + 0.5 - 8) < 6:
                c.set(x, y, None)
    c.set(7, 8, ramp("cream", 5))
    for x, y in ((22, 4), (26, 9)):
        c.set(x, y, ramp("glass", 5))
    _cloud(c, 6, 27, 24, "steel", 3.6)
    c.outline()
    c.drop_shadow()
    return c


def weather_cloudy():
    c = Canvas(WEATHER_SIZE)
    _cloud(c, 9, 17, 20, "steel", 2.6)
    _cloud(c, 2, 27, 26, "steel", 4.3)
    c.outline()
    c.drop_shadow()
    return c


def weather_rain():
    c = Canvas(WEATHER_SIZE)
    _cloud(c, 3, 20, 26, "steel", 3.4)
    _drops(c, (9, 14, 19, 24), 23, length=4)
    c.outline()
    c.drop_shadow()
    return c


def weather_snow():
    c = Canvas(WEATHER_SIZE)
    _cloud(c, 3, 20, 26, "steel", 4)
    for x, y in ((9, 24), (16, 27), (23, 24)):
        c.set(x, y, WHITE)
        for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            c.set(x + dx, y + dy, ramp("screen", 5))
    c.outline()
    c.drop_shadow()
    return c


def weather_storm():
    c = Canvas(WEATHER_SIZE)
    _cloud(c, 3, 19, 26, "graphite", 3.6)
    c.poly([(17, 19), (11, 26), (15, 26), (12, 31), (20, 23), (16, 23), (19, 19)], "amber", 4)
    _drops(c, (8, 24), 22, length=3)
    c.outline()
    c.drop_shadow()
    return c


def weather_fog():
    c = Canvas(WEATHER_SIZE)
    _cloud(c, 5, 15, 22, "steel", 3.4)
    for i, y in enumerate((19, 23, 27)):
        x0 = 4 + (i % 2) * 3
        c.rect(x0, y, x0 + 21, y + 1, "steel", 4 - i * 0.5, bevel=False, radius=0)
    c.outline()
    c.drop_shadow()
    return c


def weather_wind():
    c = Canvas(WEATHER_SIZE)
    for y, length, curl in ((10, 20, 4), (16, 25, 0), (22, 16, 3)):
        c.rect(4, y, 4 + length, y + 1, "steel", 4.2, bevel=False)
        if curl:
            cx = 4 + length
            c.ring(cx, y - 1.5 if y < 16 else y + 3.5, 3, 1.6, "steel", 4.2)
    c.outline()
    c.drop_shadow()
    return c


WEATHER = {
    "sunny": weather_sunny,
    "night": weather_night,
    "partly": weather_partly,
    "partly_night": weather_partly_night,
    "cloudy": weather_cloudy,
    "rain": weather_rain,
    "snow": weather_snow,
    "storm": weather_storm,
    "fog": weather_fog,
    "wind": weather_wind,
}


ICONS = {
    "tv": icon_tv,
    "appletv": icon_appletv,
    "audio": icon_audio,
    "home": icon_home,
    "settings": icon_settings,
}


def preview(images: dict[str, Image.Image], path: pathlib.Path) -> None:
    """Arkusz porównawczy: każda ikona w 1 x, 2 x i 6 x na tle aplikacji."""
    size = next(iter(images.values())).width
    scale_big = 6
    cell = size * scale_big + 24
    sheet = Image.new("RGBA", (cell * len(images) + 24, size * scale_big + size * 3 + 72),
                      (10, 11, 14, 255))
    for i, image in enumerate(images.values()):
        x = 24 + i * cell
        sheet.alpha_composite(image.resize((size * scale_big,) * 2, Image.NEAREST), (x, 24))
        sheet.alpha_composite(image, (x, size * scale_big + 48))
        sheet.alpha_composite(image.resize((size * 2,) * 2, Image.NEAREST),
                              (x + size + 24, size * scale_big + 48))
    sheet.save(path)


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    images = {}
    for name, draw in ICONS.items():
        image = draw().image()
        image.save(OUT / f"{name}.png")
        colors = len({p for p in image.get_flattened_data() if p[3] > 0})
        print(f"  {name}.png  {SIZE}x{SIZE}  {colors} kolorow")
        images[name] = image
    weather = {}
    for name, draw in WEATHER.items():
        image = draw().image()
        image.save(OUT / f"weather_{name}.png")
        print(f"  weather_{name}.png  {WEATHER_SIZE}x{WEATHER_SIZE}")
        weather[name] = image
    if "--preview" in sys.argv:
        target = pathlib.Path(sys.argv[sys.argv.index("--preview") + 1])
        preview(images, target)
        preview(weather, target.with_name(target.stem + "_weather" + target.suffix))
    return 0


if __name__ == "__main__":
    sys.exit(main())
