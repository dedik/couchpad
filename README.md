# Couchpad — a touch remote for TV, Apple TV, Logitech Z906 and Home Assistant: ESP32-P4 panel and Android app

[Polski](README.pl.md)

A living-room remote that controls an LG TV, an Apple TV, Logitech Z906 speakers, an air
conditioner and lights through Home Assistant over MQTT. **It comes in two versions that look
and work the same: firmware for a 4.3" ESP32-P4 touch panel and an Android app.**

| | Touch panel | Android phone |
| --- | --- | --- |
| Hardware | Guition ESP32-P4 board, 4.3" IPS 480 × 800 ([details](#hardware)) | a phone with Android 6.0 or newer (tested on a Galaxy M23) |
| Software | native firmware: ESP-IDF, C++17, LVGL 9 | native app: Kotlin, Jetpack Compose |
| Code | the root of this repository | [`phone/`](phone/README.en.md) |
| Build | [Building the panel](#building-the-panel) | [Building the app](#building-the-app) |

Both versions speak the same [MQTT contract](docs/architecture/mqtt-contract.en.md) and use the
same Home Assistant package, so you can run one, the other or both at once. Neither has a web
view: the panel wakes in about two seconds, the app shows fresh state as soon as it is opened.
The screens, settings and rules below apply to both; the panel is the reference design and the
app follows it ([ADR 0016](docs/architecture/adr/0016-android-app.en.md)).

## The panel

![All screens](docs/screenshots/overview.png)

| Start | TV | Apple TV |
| --- | --- | --- |
| ![Start](docs/screenshots/01-start.png) | ![TV](docs/screenshots/02-tv.png) | ![Apple TV](docs/screenshots/03-appletv.png) |
| **Speakers (Z906)** | **Home** | **Settings** |
| ![Z906](docs/screenshots/04-audio.png) | ![Home](docs/screenshots/05-home.png) | ![Settings](docs/screenshots/06-more.png) |
| **Apple TV keyboard — QWERTY** | **Apple TV keyboard — Apple TV layout** | |
| ![QWERTY keyboard](docs/screenshots/07-keyboard-qwerty.png) | ![Apple TV layout keyboard](docs/screenshots/08-keyboard-alt.png) | |

### Light theme

The same interface in the light theme — permanently, or during the day when the theme follows the clock.

![All screens — light theme](docs/screenshots/overview-light.png)

| Keyboard — QWERTY | Keyboard — Apple TV layout |
| --- | --- |
| ![QWERTY keyboard, light theme](docs/screenshots/light/07-keyboard-qwerty.png) | ![Apple TV layout keyboard, light theme](docs/screenshots/light/08-keyboard-alt.png) |

Individual light-theme screens are in [`docs/screenshots/light/`](docs/screenshots/light/).

### Landscape

The same remote held sideways or standing on a table: 800 × 480 with tabs in a side rail,
two-column layouts and the selectors moved into the header. Chosen in *Settings → Display →
Orientation*; the remote restarts in the new orientation
([ADR 0013](docs/architecture/adr/0013-landscape-orientation.en.md)).

![All screens — landscape](docs/screenshots/overview-landscape.png)

![All screens — landscape, light theme](docs/screenshots/overview-landscape-light.png)

Individual landscape screens are in [`docs/screenshots/landscape/`](docs/screenshots/landscape/)
and [`docs/screenshots/landscape/light/`](docs/screenshots/landscape/light/).

### Home tab

Rooms, a room, the Lamp, the effect list, a single bulb and — in landscape — the room drawer
that slides in with an edge swipe. The rules new devices follow:
[`docs/ux/home-screen.en.md`](docs/ux/home-screen.en.md) ([ADR 0014](docs/architecture/adr/0014-rooms-and-lights.en.md)).

![Home tab — portrait](docs/screenshots/overview-home.png)

![Home tab — portrait, light theme](docs/screenshots/overview-home-light.png)

![Home tab — landscape](docs/screenshots/overview-home-landscape.png)

![Home tab — landscape, light theme](docs/screenshots/overview-home-landscape-light.png)

The interface is in Polish. All texts live in `components/ui/screens/*.cpp`.

## The Android app

The same screens on a phone — for example an old one set as the home screen and left on the
couch. The app can replace the phone's launcher: it starts with the phone, Home returns to the
start screen and it shows up as soon as the screen turns on. Details, build and status:
[`phone/README.en.md`](phone/README.en.md).

![Android app — portrait](phone/docs/screenshots/overview-portrait.png)

![Android app — landscape](phone/docs/screenshots/overview-landscape.png)

![Android app — portrait, light theme](phone/docs/screenshots/overview-portrait-light.png)

![Android app — landscape, light theme](phone/docs/screenshots/overview-landscape-light.png)

## What it does

Both versions have the same screens and settings:

- **TV** — power state, input and picture mode, D-pad, volume and channel rockers, four
  channel shortcuts with logos and a list of all tuner channels.
- **Apple TV** — app tiles, Menu/Home and a round clickpad that works like the Siri
  Remote: tap an arrow or the centre for a single press, or swipe anywhere on the circle
  and it keeps stepping through lists as your finger moves (sensitivity in settings).
- **Hidden Siri Remote gestures** — the same ones as on the original remote: double-tap
  Home for the app switcher, hold Home for Control Center, hold the centre of the clickpad
  for an icon's context menu ([details](docs/integrations/appletv.en.md)).
- **Apple TV keyboard** — slides up by itself when a text field appears on the Apple TV
  (search, login), types live as you go (Polish characters included) and can also be
  opened with the **Tekst** button. Two layouts: QWERTY or the alphabetical grid known from
  the Apple TV, switchable on the keyboard itself or in settings. It uses Home Assistant's
  `apple_tv.set_keyboard_text` and the `keyboard_focus` sensor ([ADR 0007](docs/architecture/adr/0007-apple-tv-via-home-assistant.en.md)).
- **Z906** — volume dial with −/+ steps, centre, subwoofer and rear levels, input and
  effect, Decode indicator.
- **Home** — six rooms with pixel-art icons and a quick main-light toggle; inside a room its
  devices: the Lamp (brightness dial, colour temperature, 16 effects, each bulb separately)
  and the air conditioner (mode, temperature, fan, swing). Tab standard:
  [`docs/ux/home-screen.en.md`](docs/ux/home-screen.en.md).
- **Start screen** after wake-up — clock, weather and the state of every device.
- **Light and dark theme** — chosen in the settings: fixed or by hours (dark from 18 to 8 by
  default). Switches at runtime, without a restart
  ([ADR 0012](docs/architecture/adr/0012-light-and-dark-theme.en.md)).
- **Portrait or landscape** — a setting, not a sensor (the board has none): in landscape the
  image is rotated on the CPU and every screen has its own two-column layout
  ([ADR 0013](docs/architecture/adr/0013-landscape-orientation.en.md)). The app can also
  follow the phone's rotation.
- **Settings on the device** — Wi-Fi, MQTT broker, screen and theme, sound limit, room and time zone,
  keyboard and trackpad, diagnostics. Credentials are entered on the device (the panel's NVS,
  the app's private storage), never in the repository. On the phone Wi-Fi is the system's.

The remote never shows stale state as current: after waking up it asks for a fresh snapshot
and greys out controls until it arrives
([ADR 0005](docs/architecture/adr/0005-fresh-state-after-wake.en.md)).

## Hardware

| Part | Notes |
| --- | --- |
| [Guition JC4880P443C_I_W (ESP32-P4 + ESP32-C6, 4.3" IPS 480×800, touch, case)](https://pl.aliexpress.com/item/1005009675498892.html) | ESP32-P4 runs the app, ESP32-C6 provides Wi-Fi over SDIO. Pins and timings verified on a real unit: [docs/hardware/verified-board-map.md](docs/hardware/verified-board-map.en.md) |
| USB-C cable | power, flashing, logs, screenshots |

The camera on the board is not used. The Android app needs only a phone on the same network
as the MQTT broker.

## How the pieces talk

```text
                       ┌── Glasshouse on the LG TV      (TV state and keys)
panel or app ── MQTT ──┼── Z906 controller (ESP32)       (speakers, direct topics)
                       └── Home Assistant ── Apple TV, AC, weather, TV channel switching
```

| Device | Integration | Path |
| --- | --- | --- |
| LG TV (webOS) | [Glasshouse](https://github.com/rorygallagher2024/glasshouse) running on the TV | MQTT directly; channel shortcuts go through Home Assistant, which calls Glasshouse over HTTP ([ADR 0008](docs/architecture/adr/0008-tv-via-glasshouse.en.md)) |
| Logitech Z906 | ESP32 controller based on [ggroohauga](https://github.com/nomis/ggroohauga), modified to publish MQTT | MQTT directly, topics `z906/<id>/…` ([ADR 0006](docs/architecture/adr/0006-direct-z906-topics.en.md)) |
| Apple TV | Home Assistant `apple_tv` integration | `multimedia_remote/v1` contract through the HA package ([ADR 0007](docs/architecture/adr/0007-apple-tv-via-home-assistant.en.md)) |
| Air conditioner | any Home Assistant `climate` entity | same contract ([ADR 0009](docs/architecture/adr/0009-climate-via-home-assistant.en.md)) |
| Weather | any Home Assistant `weather` entity | same contract ([ADR 0011](docs/architecture/adr/0011-weather-on-start-screen.en.md)) |

The full message format is in [docs/architecture/mqtt-contract.md](docs/architecture/mqtt-contract.en.md).
Every integration is optional: a screen without a source shows a "no fresh state" banner.

## Build

### Building the panel

Requirements: [ESP-IDF v5.5](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32p4/get-started/)
(tested with v5.5.6). Component versions are pinned in `main/idf_component.yml`.

```bash
. ~/esp/esp-idf/export.sh
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor
```

Use your own serial port. Before the first flash you may want a backup of the factory
firmware: `esptool.py --port <port> read_flash 0 0x1000000 backup.bin`.

### Building the app

Requirements: JDK 17 and the Android SDK (`platforms;android-34`, `build-tools;34.0.0`).

```bash
cd phone/native
./gradlew assembleDebug
adb install -r app/build/outputs/apk/debug/app-debug.apk
```

Full steps, tests and the older SDL/LVGL port: [`phone/README.en.md`](phone/README.en.md).

## Make it yours

### 1. On the device (no code changes)

Open **Settings** (gear tab) on the panel or in the app — they are the same:

1. **Wi-Fi** — pick the network and type the password (panel only; the phone uses its own Wi-Fi).
2. **MQTT** — broker address, port, user and password, the remote's identifier (used in
   `multimedia_remote/v1/<identifier>`), the Z906 controller id (`z906_<mac>`) and the
   Glasshouse topic prefix.
3. **Room and time** — room name shown in headers, time zone.
4. **Display** (optional) — dark, light or by-hours theme, brightness, screen timeout.

Create a separate MQTT user for the remote.

### 2. Home Assistant

Install the package from [homeassistant/](homeassistant/README.en.md):
copy `multimedia_remote.yaml` to `packages/`, replace the example entity ids with yours,
set the remote identifier, and put the Glasshouse address and token in `secrets.yaml`.

### 3. Z906 controller

The remote expects one value per topic under `z906/<id>/state/<name>` and sends commands
to `z906/<id>/command/<name>` — the full table is in
[ADR 0006](docs/architecture/adr/0006-direct-z906-topics.en.md). If your controller publishes
a different layout, adapt `components/transport/z906_client.cpp`.

### 4. In the code

For the panel (the app keeps its equivalents in `phone/native/app/src/main/java/app/couchpad/pilot/`):

| What | Where |
| --- | --- |
| Channel shortcuts (names as the tuner reports them) | `kChannelIds` in `components/ui/screens/screen_tv.cpp` |
| Channel and app logos | SVGs in `docs/ux/mockup-pulpit/assets/`, sizes in `tools/gen_assets.py`, then `./tools/gen-assets.sh` |
| Apple TV app tiles (bundle ids) | `components/domain/include/domain/appletv_apps.hpp` and `atv_favourites` in the HA package |
| Device names in headers ("LG OLED", "Z906") | `kScreens` in `components/ui/shell.cpp` |
| Default room name and time zone | `components/storage/settings.cpp` |
| UI texts | `components/ui/screens/*.cpp` |

## Development tools

- **USB console** (`CONFIG_PILOT_USB_CONSOLE`, on by default): screenshots and remote touch
  over the USB cable.

  ```bash
  uvx --with pyserial --with pillow python tools/remote.py shot screen.png
  uvx --with pyserial --with pillow python tools/remote.py tour screenshots/
  ```

  `tour` only taps the tab bar. The settings screen shows your Wi-Fi name — mask it before
  publishing a screenshot.
- **Log over MQTT** (`CONFIG_PILOT_REMOTE_LOG`, off by default): `tools/pilot_log.py`.
- **Interactive mockup** of the whole UI: `docs/ux/mockup-pulpit/index.html`.
- **Android app screenshots** from a phone connected over adb:
  `phone/tools/screenshots.sh portrait|landscape [light]`.

## TODO

- [x] **Lights from Home Assistant** — the main light of each room, bulb groups and effects,
  configured in the bridge (`light_rooms`), not in the code ([ADR 0014](docs/architecture/adr/0014-rooms-and-lights.en.md)).
- [ ] **More Home Assistant entities** — covers, sensors, sockets and other devices on the Home
  screen following the [tab standard](docs/ux/home-screen.en.md).
- [ ] **Battery power** — today the remote runs from USB only. Planned: a battery and a
  rear case, deep sleep between uses with touch wake-up, battery level on screen and
  charging over USB-C.

## Documentation

Design documents are written in Polish, each with an English version next to it
(`*.en.md`):
[architecture and ADRs](docs/architecture/README.en.md) ·
[UI architecture](docs/architecture/ui-architecture.en.md) ·
[board bring-up](docs/hardware/bring-up.en.md) ·
[integrations](docs/integrations/) ·
[Android app](phone/README.en.md) ([status, in Polish](phone/docs/PROGRESS.md)) ·
[notes for contributors and AI agents](AGENTS.en.md).

## License

[MIT](LICENSE) for the code in this repository. Bundled third-party parts keep their own
licenses:

- `components/board_bsp` — Espressif BSP, Apache-2.0 (`components/board_bsp/LICENSE`)
- `components/esp_lcd_st7701_patched` — Espressif ST7701 driver with a patched init
  sequence, Apache-2.0 (`license.txt`)
- Ubuntu font in `components/ui/assets/fonts` and `phone/native/app/src/main/res/font` —
  Ubuntu Font Licence 1.0
- cJSON in `phone/third_party/cJSON` (SDL port) — MIT
- Infuse and YouTube marks from [Simple Icons](https://simpleicons.org) (CC0)

Channel and app logos (Polsat, TVN, TVP, WP Pilot, Infuse, YouTube) are trademarks of
their owners and are used only to identify the shortcuts.
