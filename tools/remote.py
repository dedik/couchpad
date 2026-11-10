#!/usr/bin/env python3
"""Steruje pilotem przez konsolę USB: zrzuty ekranu i zdalny dotyk.

Firmware musi mieć włączone CONFIG_PILOT_USB_CONSOLE (menu „Pilot”). Port otwiera
się bez restartu pilota.

    uvx --with pyserial --with pillow python tools/remote.py shot zrzut.png
    uvx --with pyserial --with pillow python tools/remote.py tap 240 400
    uvx --with pyserial --with pillow python tools/remote.py drag 240 600 240 300 300
    uvx --with pyserial --with pillow python tools/remote.py hit 166 420
    uvx --with pyserial --with pillow python tools/remote.py tour katalog/

`tour` przechodzi przez zakładki i podstrony i zapisuje zrzut każdej z nich
(tylko w pionie — współrzędne zakładek są pionowe).

Przy orientacji poziomej (Ekran → Orientacja) `shot` zapisuje obraz obrócony tak,
jak widzi go użytkownik, a `tap`/`drag` przyjmują współrzędne logiczne 800 x 480.
Port domyślnie `/dev/cu.usbmodem101`; inny podaje zmienna PILOT_PORT.
"""

from __future__ import annotations

import base64
import os
import pathlib
import sys
import time
import zlib

import serial
from PIL import Image

TIMEOUT_S = 15.0

# Środki zakładek na ekranie 480 x 800 (components/ui/shell.cpp, theme.hpp): pasek
# ma 80 px, sześć zakładek po ok. 71 px z odstępem 4 px i marginesem 16 px (Pilot
# pierwszy, ADR 0020). Zakładka tylko przełącza ekran, niczego nie wysyła do urządzeń —
# `tour` dotyka wyłącznie paska.
TABS = {"remote": 52, "tv": 127, "appletv": 202, "audio": 278, "home": 353, "more": 428}
TAB_Y = 760
# Budzenie: na ekranie startowym w tym miejscu jest napis o połączeniu, na pozostałych
# zakładka Głośniki. Ani jedno, ani drugie niczego nie przełącza w domu.
WAKE = (240, 760)
# Ekran startowy (ADR 0010) leży nad zakładkami. Kafelek Telewizor tylko go zamyka
# i pokazuje ekran TV — niczego nie wysyła.
START_TILE_TV = (120, 300)

# Zrzuty do README. Pilot po wybudzeniu pokazuje ekran startowy (ADR 0010), więc
# `tour` najlepiej uruchomić, gdy ekran jest wygaszony.
# Uwaga: ekran „more” pokazuje nazwę sieci Wi-Fi — przed publikacją ją zamaskuj.
TOUR = [
    ("01-start", None),
    ("02-tv", "tv"),
    ("15-remote", "remote"),
    ("03-appletv", "appletv"),
    ("04-audio", "audio"),
    ("05-home", "home"),
    ("06-more", "more"),
]


class Remote:
    def __init__(self, device: str) -> None:
        self.port = serial.Serial()
        self.port.port = device
        self.port.baudrate = 115200
        self.port.timeout = 0.5
        self.port.open()
        # macOS podnosi DTR i RTS przy otwarciu. USB-Serial-JTAG resetuje układ przy
        # RTS=1 i DTR=0, więc RTS musi opaść pierwszy — inaczej każde wywołanie to restart.
        self.port.rts = False
        self.port.dtr = False
        self.port.reset_input_buffer()

    def close(self) -> None:
        self.port.close()

    def _line(self, deadline: float) -> bytes:
        line = self.port.readline()
        if not line and time.monotonic() > deadline:
            raise TimeoutError("pilot nie odpowiedział — czy firmware ma CONFIG_PILOT_USB_CONSOLE?")
        return line

    def _command(self, text: str) -> None:
        self.port.write(f"\n{text}\n".encode())
        deadline = time.monotonic() + TIMEOUT_S
        while not self._line(deadline).startswith(b"@@OK"):
            pass

    def tap(self, x: int, y: int) -> None:
        self._command(f"tap {x} {y}")

    def hit(self, x: int, y: int) -> str:
        """Który obiekt LVGL dostałby dotyk w (x, y) — nic nie dotyka."""
        self.port.write(f"\nhit {x} {y}\n".encode())
        deadline = time.monotonic() + TIMEOUT_S
        found = ""
        while True:
            line = self._line(deadline)
            if line.startswith(b"@@HIT"):
                found = line.decode(errors="replace").strip()
            elif line.startswith(b"@@OK"):
                return found

    def drag(self, x1: int, y1: int, x2: int, y2: int, ms: int) -> None:
        self._command(f"drag {x1} {y1} {x2} {y2} {ms}")

    def shot(self) -> Image.Image:
        self.port.write(b"\nshot\n")
        deadline = time.monotonic() + TIMEOUT_S
        while True:
            line = self._line(deadline)
            if line.startswith(b"@@SHOT "):
                break
        fields = line.split()
        _, width, height, fmt, size = fields[:5]
        # Szóste pole (obrót interfejsu) dodał firmware z orientacją poziomą.
        degrees = int(fields[5]) if len(fields) > 5 else 0
        if fmt != b"RGB565":
            raise ValueError(f"nieznany format {fmt!r}")
        width, height, size = int(width), int(height), int(size)

        chunks: list[bytes] = []
        while True:
            line = self._line(deadline).strip()
            if line.startswith(b"@@END "):
                crc = int(line.split()[1], 16)
                break
            if line:
                chunks.append(base64.b64decode(line))
        while not self._line(deadline).startswith(b"@@OK"):
            pass

        data = b"".join(chunks)
        if len(data) != size:
            raise ValueError(f"odebrano {len(data)} z {size} bajtów")
        if zlib.crc32(data) != crc:
            raise ValueError("CRC się nie zgadza — ramka uszkodzona po drodze")
        return rotate(to_image(data, width, height), degrees)


# Bufor ramki DPI jest zawsze pionowy. LVGL przy obrocie o 90° kładzie piksel logiczny
# (x, y) w (y, 799 - x) bufora, więc obraz dla oka to obrót bufora zgodnie z zegarem.
ROTATIONS = {
    0: None,
    90: Image.Transpose.ROTATE_270,
    270: Image.Transpose.ROTATE_90,
}


def rotate(image: Image.Image, degrees: int) -> Image.Image:
    if degrees not in ROTATIONS:
        raise ValueError(f"nieznany obrót {degrees}")
    method = ROTATIONS[degrees]
    return image if method is None else image.transpose(method)


def to_image(data: bytes, width: int, height: int) -> Image.Image:
    # RGB565 little-endian: R w bitach 15–11, G 10–5, B 4–0.
    pixels = bytearray(width * height * 3)
    for i in range(width * height):
        value = data[2 * i] | (data[2 * i + 1] << 8)
        r, g, b = value >> 11, (value >> 5) & 0x3F, value & 0x1F
        pixels[3 * i] = (r << 3) | (r >> 2)
        pixels[3 * i + 1] = (g << 2) | (g >> 4)
        pixels[3 * i + 2] = (b << 3) | (b >> 2)
    return Image.frombytes("RGB", (width, height), bytes(pixels))


def main() -> int:
    if len(sys.argv) < 2 or sys.argv[1] not in {"shot", "tap", "drag", "tour", "hit"}:
        print(__doc__)
        return 2
    action, args = sys.argv[1], sys.argv[2:]
    remote = Remote(os.environ.get("PILOT_PORT", "/dev/cu.usbmodem101"))
    try:
        if action == "shot":
            out = args[0] if args else "screenshot.png"
            started = time.monotonic()
            remote.shot().save(out)
            print(f"{out}  {time.monotonic() - started:.1f} s")
        elif action == "tap":
            remote.tap(int(args[0]), int(args[1]))
        elif action == "hit":
            print(remote.hit(int(args[0]), int(args[1])))
        elif action == "drag":
            remote.drag(*(int(value) for value in args[:5]))
        else:
            folder = pathlib.Path(args[0] if args else "screenshots")
            folder.mkdir(parents=True, exist_ok=True)
            remote.tap(*WAKE)
            time.sleep(3)  # świeży stan po wybudzeniu (ADR 0005)
            for name, tab in TOUR:
                if tab is not None and name == TOUR[1][0]:
                    remote.tap(*START_TILE_TV)
                    time.sleep(1.2)
                if tab is not None:
                    remote.tap(TABS[tab], TAB_Y)
                    time.sleep(1.2)  # animacja przejścia
                remote.shot().save(folder / f"{name}.png")
                print(folder / f"{name}.png")
    finally:
        remote.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
