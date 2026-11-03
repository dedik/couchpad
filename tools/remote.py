#!/usr/bin/env python3
"""Steruje pilotem przez konsolę USB: zrzuty ekranu i zdalny dotyk.

Firmware musi mieć włączone CONFIG_PILOT_USB_CONSOLE (menu „Pilot”). Port otwiera
się bez restartu pilota.

    uvx --with pyserial --with pillow python tools/remote.py shot zrzut.png
    uvx --with pyserial --with pillow python tools/remote.py tap 240 400
    uvx --with pyserial --with pillow python tools/remote.py drag 240 600 240 300 300
    uvx --with pyserial --with pillow python tools/remote.py tour katalog/

`tour` przechodzi przez zakładki i podstrony i zapisuje zrzut każdej z nich.
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
# ma 80 px, pięć zakładek po 86 px z marginesem 16 px. Zakładka tylko przełącza ekran,
# niczego nie wysyła do urządzeń — `tour` dotyka wyłącznie paska.
TABS = {"tv": 59, "appletv": 150, "audio": 240, "home": 330, "more": 421}
TAB_Y = 760
# Budzenie: na ekranie startowym w tym miejscu jest napis o połączeniu, na pozostałych
# zakładka Głośniki. Ani jedno, ani drugie niczego nie przełącza w domu.
WAKE = (240, 760)

# Zrzuty do README. Pilot po wybudzeniu pokazuje ekran startowy (ADR 0010), więc
# `tour` najlepiej uruchomić, gdy ekran jest wygaszony.
# Uwaga: ekran „more” pokazuje nazwę sieci Wi-Fi — przed publikacją ją zamaskuj.
TOUR = [
    ("01-start", None),
    ("02-tv", "tv"),
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

    def drag(self, x1: int, y1: int, x2: int, y2: int, ms: int) -> None:
        self._command(f"drag {x1} {y1} {x2} {y2} {ms}")

    def shot(self) -> Image.Image:
        self.port.write(b"\nshot\n")
        deadline = time.monotonic() + TIMEOUT_S
        while True:
            line = self._line(deadline)
            if line.startswith(b"@@SHOT "):
                break
        _, width, height, fmt, size = line.split()
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
        return to_image(data, width, height)


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
    if len(sys.argv) < 2 or sys.argv[1] not in {"shot", "tap", "drag", "tour"}:
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
        elif action == "drag":
            remote.drag(*(int(value) for value in args[:5]))
        else:
            folder = pathlib.Path(args[0] if args else "screenshots")
            folder.mkdir(parents=True, exist_ok=True)
            remote.tap(*WAKE)
            time.sleep(3)  # świeży stan po wybudzeniu (ADR 0005)
            for name, tab in TOUR:
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
