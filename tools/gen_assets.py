#!/usr/bin/env python3
"""Zamienia zasoby makiety Pulpitu na obrazy LVGL.

Trzy rodziny zasobów, trzy formaty dobrane do tego, czym naprawdę są:

* ikony zakładek — pixel art 48 x 48 z `assets/pixel/*.png` (rysuje je
  tools/draw_icons.py), zapisany jako obraz indeksowany `I8`: kilkadziesiąt barw z
  półprzezroczystym cieniem, bez wygładzania. Zakładka bierze go w skali 1 x
  (`icon_<nazwa>`, 48 px), kafelek ekranu startowego w 2 x (`icon_<nazwa>_large`, 96 px);
* logotypy — SVG rasteryzowany Inkscape'em do docelowego rozmiaru, format `RGB565A8`,
  bo każdy ma przezroczyste tło;
* plakaty — SVG w 100 x 150, format `RGB565` bez kanału alfa, bo zawsze zasłaniają tło.

Uruchomienie:  ./tools/gen-assets.sh
"""

from __future__ import annotations

import pathlib
import re
import subprocess
import sys
import tempfile

from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parent.parent
MOCKUP = ROOT / "docs" / "ux" / "mockup-pulpit"
OUT = ROOT / "components" / "ui" / "assets" / "images"

INKSCAPE = "/Applications/Inkscape.app/Contents/MacOS/inkscape"

# Logotypy: docelowa wysokość w pikselach. Znak kwadratowy musi być wyższy niż
# napis szeroki — tak samo jak w makiecie, gdzie wyrównuje się je optycznie.
LOGOS = {
    "polsat": 38,
    "tvn": 38,
    "tvp1": 22,
    "tvp2": 22,
    "infuse": 38,
    "youtube": 38,
    "wppilot": 22,   # napis, nie znak — wysokość jak TVP
}

TAB_ICONS = ["tv", "appletv", "audio", "home", "settings"]
PIXEL_SCALE = 1
# Kafelki ekranu startowego: te same obrazy, każdy piksel na 2 x 2 piksele ekranu.
LARGE_ICONS = ["tv", "appletv", "audio", "home"]
LARGE_SCALE = 2
# Pogoda na ekranie startowym (ADR 0011): siatka 32 x 32 w skali 2 x, 64 px obok zegara.
WEATHER_ICONS = ["sunny", "night", "partly", "partly_night", "cloudy", "rain", "snow", "storm", "fog", "wind"]
WEATHER_SCALE = 2
# Ekran Dom (ADR 0014, rysuje tools/draw_room_icons.py): pomieszczenie 48 px w pasku
# pomieszczeń i 96 px na kafelku; lampa 64 px; żarówka 32 px w wierszu i 64 px w nagłówku.
ROOM_ICONS = ["salon", "kuchnia", "przedpokoj", "biuro", "lazienka", "sypialnia"]
LAMP_STATES = ["on", "dim", "off", "na"]
BULB_STATES = ["on", "off"]
# Ikony opcji (tools/draw_option_icons.py, siatka 24 x 24): efekty lampy i tryby klimy.
# Mały kafel i wiersz listy w 1 x (24 px), duży kafel bieżącej opcji w 2 x (48 px).
EFFECT_ICONS = ["none", "cozy", "relax", "tv", "candle", "warm", "golden", "daylight", "cool", "focus",
                "night", "bedtime", "wakeup", "pulse", "steampunk", "diwali", "alarm", "more"]
MODE_ICONS = ["auto", "cool", "heat", "fan_only", "dry"]
# Urządzenia i czujniki (tools/draw_device_icons.py): klimatyzator 48 px w studzience karty,
# termometr i kropla (siatka 24) w 2 x — 48 px na karcie „W pokoju” (ADR 0017).
DEVICE_ICONS = ["aircon", "radiator", "radiator_idle"]
SENSOR_ICONS = ["temp", "humidity"]
# Głowica grzejnika (ADR 0018, tools/draw_option_icons.py, siatka 24): tryb Harmonogram i kafle
# „Teraz” (zawór, okno, bateria, następny punkt, cel – pokój) w 1 x. Tryb Grzanie bierze `mode_heat`.
TRV_ICONS = ["auto", "valve_on", "valve_off", "window_open", "window_closed", "battery_ok", "battery_low",
             "next", "delta"]
# Tarcza grzejnika: pierścień od niebieskiego (zimno, początek łuku 135°) przez bursztyn do
# czerwieni (gorąco, koniec 45°) — tekstura łuku `lv_arc` (arc_image_src). Rozmiar = kThermoSize.
RING_SIZE = 300
RING_STOPS = [(0.0, (0x3D, 0x8B, 0xFF)), (0.5, (0xF2, 0xA9, 0x3B)), (1.0, (0xE5, 0x28, 0x1B))]


def c_identifier(name: str) -> str:
    return re.sub(r"[^0-9a-zA-Z]", "_", name)


def emit_i8(name: str, image: Image.Image) -> str:
    """Obraz `I8`: paleta 256 kolorów ARGB (indeks 0 przezroczysty), bajt na piksel."""
    width, height = image.size
    palette: list[tuple[int, int, int, int]] = [(0, 0, 0, 0)]
    indices: list[int] = []
    for pixel in image.convert("RGBA").get_flattened_data():
        if pixel[3] == 0:
            indices.append(0)
            continue
        if pixel not in palette:
            palette.append(pixel)
        indices.append(palette.index(pixel))
    if len(palette) > 256:
        raise ValueError(f"{name}: {len(palette)} kolorow, I8 miesci 256")
    out = [f"    /* paleta: {len(palette)} z 256 kolorow */"]
    for red, green, blue, alpha in palette + [(0, 0, 0, 0)] * (256 - len(palette)):
        out.append(f"    0x{blue:02x}, 0x{green:02x}, 0x{red:02x}, 0x{alpha:02x},")
    out.append("    /* piksele */")
    for y in range(height):
        row = indices[y * width:(y + 1) * width]
        out.append("    " + " ".join(f"0x{value:02x}," for value in row))
    return render_c(name, width, height, width, "LV_COLOR_FORMAT_I8", 256 * 4 + width * height,
                    "\n".join(out))


def emit_rgb565a8(name: str, image: Image.Image) -> str:
    """`RGB565A8`: najpierw cała mapa RGB565, potem cała mapa alfy."""
    width, height = image.size
    pixels = image.convert("RGBA").load()

    colour_lines = []
    for y in range(height):
        values = []
        for x in range(width):
            red, green, blue, _ = pixels[x, y]
            value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            values.append(f"0x{value & 0xFF:02x}, 0x{value >> 8:02x},")
        colour_lines.append("    " + " ".join(values))

    alpha_lines = ["    /* kanal alfa */"]
    for y in range(height):
        values = [f"0x{pixels[x, y][3]:02x}," for x in range(width)]
        alpha_lines.append("    " + " ".join(values))

    stride = width * 2
    data_size = width * height * 3
    body = "\n".join(colour_lines + alpha_lines)
    return render_c(name, width, height, stride, "LV_COLOR_FORMAT_RGB565A8", data_size, body)


def emit_rgb565(name: str, image: Image.Image) -> str:
    width, height = image.size
    pixels = image.convert("RGB").load()
    lines = []
    for y in range(height):
        values = []
        for x in range(width):
            red, green, blue = pixels[x, y]
            value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
            values.append(f"0x{value & 0xFF:02x}, 0x{value >> 8:02x},")
        lines.append("    " + " ".join(values))

    stride = width * 2
    return render_c(name, width, height, stride, "LV_COLOR_FORMAT_RGB565", width * height * 2,
                    "\n".join(lines))


def render_c(name: str, width: int, height: int, stride: int, fmt: str, data_size: int,
             body: str) -> str:
    ident = c_identifier(name)
    return f"""/* Wygenerowane przez tools/gen_assets.py — nie poprawiac recznie.
   Zrodlo: docs/ux/mockup-pulpit/. */
#include "lvgl.h"

static const uint8_t {ident}_map[] = {{
{body}
}};

const lv_image_dsc_t {ident} = {{
    .header = {{
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = {fmt},
        .flags = 0,
        .w = {width},
        .h = {height},
        .stride = {stride},
        .reserved_2 = 0,
    }},
    .data_size = {data_size},
    .data = {ident}_map,
    .reserved = NULL,
}};
"""


def heat_ring() -> Image.Image:
    """Pierścień barw tarczy grzejnika: kolor zależy od kąta wzdłuż łuku 270° (od 135° w prawo)."""
    import math
    image = Image.new("RGB", (RING_SIZE, RING_SIZE))
    pixels = image.load()
    c = (RING_SIZE - 1) / 2
    for y in range(RING_SIZE):
        for x in range(RING_SIZE):
            angle = math.degrees(math.atan2(y - c, x - c)) % 360   # 0° = w prawo, rośnie w dół
            t = ((angle - 135) % 360) / 270
            t = min(1.0, t) if t <= 1.0 + 45 / 270 else 0.0          # przerwa u dołu: najbliższy koniec
            for (a, ca), (b, cb) in zip(RING_STOPS, RING_STOPS[1:]):
                if a <= t <= b:
                    k = (t - a) / (b - a)
                    pixels[x, y] = tuple(round(ca[i] + (cb[i] - ca[i]) * k) for i in range(3))
                    break
    return image


def render_svg(svg: pathlib.Path, width: int, height: int) -> Image.Image:
    with tempfile.TemporaryDirectory() as tmp:
        png = pathlib.Path(tmp) / "out.png"
        subprocess.run(
            [INKSCAPE, str(svg), "--export-type=png", f"--export-filename={png}",
             f"--export-width={width}", f"--export-height={height}",
             "--export-background-opacity=0"],
            check=True, capture_output=True)
        return Image.open(png).convert("RGBA").copy()


def svg_aspect(svg: pathlib.Path) -> float:
    text = svg.read_text(encoding="utf-8")
    box = re.search(r'viewBox="([^"]+)"', text).group(1)
    numbers = [float(value) for value in re.split(r"[\s,]+", box.strip())]
    return numbers[2] / numbers[3]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    names: list[str] = []

    # --- ikony zakładek i kafelków startowych ---------------------------------
    jobs = [(icon, PIXEL_SCALE, f"icon_{icon}") for icon in TAB_ICONS]
    jobs += [(icon, LARGE_SCALE, f"icon_{icon}_large") for icon in LARGE_ICONS]
    jobs += [(f"weather_{icon}", WEATHER_SCALE, f"icon_weather_{icon}") for icon in WEATHER_ICONS]
    jobs += [(f"room_{room}", 1, f"icon_room_{room}") for room in ROOM_ICONS]
    jobs += [(f"room_{room}", 2, f"icon_room_{room}_large") for room in ROOM_ICONS]
    jobs += [(f"lamp_{state}", 2, f"icon_lamp_{state}") for state in LAMP_STATES]
    jobs += [(f"bulb_{state}", 1, f"icon_bulb_{state}") for state in BULB_STATES]
    jobs += [(f"bulb_{state}", 2, f"icon_bulb_{state}_large") for state in BULB_STATES]
    for kind, icons in (("fx", EFFECT_ICONS), ("mode", MODE_ICONS)):
        jobs += [(f"{kind}_{icon}", 1, f"icon_{kind}_{icon}") for icon in icons]
        jobs += [(f"{kind}_{icon}", 2, f"icon_{kind}_{icon}_large") for icon in icons]
    jobs += [(f"dev_{icon}", 1, f"icon_dev_{icon}") for icon in DEVICE_ICONS]
    jobs += [(f"sensor_{icon}", 2, f"icon_sensor_{icon}_large") for icon in SENSOR_ICONS]
    jobs += [(f"trv_{icon}", 1, f"icon_trv_{icon}") for icon in TRV_ICONS]
    for icon, scale, name in jobs:
        source = Image.open(MOCKUP / "assets" / "pixel" / f"{icon}.png").convert("RGBA")
        image = source.resize((source.width * scale, source.height * scale), Image.NEAREST)
        (OUT / f"{name}.c").write_text(emit_i8(name, image))
        names.append(name)
        print(f"  {name}  {image.width}x{image.height}  I8")

    # --- tarcza grzejnika -------------------------------------------------------
    (OUT / "ring_heat.c").write_text(emit_rgb565("ring_heat", heat_ring()))
    names.append("ring_heat")
    print(f"  ring_heat  {RING_SIZE}x{RING_SIZE}  RGB565")

    # --- logotypy -------------------------------------------------------------
    for logo, target_height in LOGOS.items():
        svg = MOCKUP / "assets" / f"{logo}.svg"
        width = max(1, round(target_height * svg_aspect(svg)))
        image = render_svg(svg, width, target_height)
        name = f"logo_{logo}"
        (OUT / f"{name}.c").write_text(emit_rgb565a8(name, image))
        names.append(name)
        print(f"  {name}  {width}x{target_height}  RGB565A8")

    header = ["/* Wygenerowane przez tools/gen_assets.py — nie poprawiac recznie. */",
              "#pragma once", "", '#include "lvgl.h"', "",
              "#ifdef __cplusplus", 'extern "C" {', "#endif", ""]
    header += [f"LV_IMAGE_DECLARE({name});" for name in names]
    header += ["", "#ifdef __cplusplus", "}", "#endif", ""]
    (OUT / "images.h").write_text("\n".join(header))
    print(f"Gotowe: {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
