# Components

[Polski](README.md)

## Board support, taken over from the manufacturer

Two directories come from the Guition example for the `JC4880P443C_I_W` model and are here **deliberately**, even though
they look like external dependencies:

| Directory | What it is |
| --- | --- |
| `board_bsp/` | Espressif's BSP for the ESP32-P4 Function EV Board, adapted by the manufacturer for this board |
| `esp_lcd_st7701_patched/` | ST7701 driver with the manufacturer's **own initialisation sequence** |

**Do not replace them with versions from the component registry.** The standard `esp_lcd_st7701` has a different initialisation
sequence and produces a black screen on this panel. This is confirmed both by the manufacturer's example and
by an independent ESPHome implementation for this model.

### Change made by us

In `board_bsp/esp32_p4_function_ev_board.c`, the setting of the PLL clock source for the PHY
in MIPI-DSI has been removed. The original passed a compatibility constant there meaning `PLL_F20M`, which on chips from revision
3.0 upwards ends in `abort()` and a restart loop before the screen even lights up. The reason and the full
diagnosis are in the code comment and in [`../docs/hardware/verified-board-map.md`](../docs/hardware/verified-board-map.en.md).

### Misleading strings in the manufacturer's code

- `Install EK79007 LCD control panel` — not true, this block calls `esp_lcd_new_panel_st7701()`.
- The macro `ST7701_480_360_PANEL_60HZ_DPI_CONFIG` has 480 × 360 in its name, but its contents were
  rewritten by the manufacturer for 480 × 800.
- The Kconfig option `BSP_LCD_TYPE_1024_600`, described as "LCD 7-inch 1024x600 - ek79007", selects
  the ST7701 480 × 800 branch in this copy. **This is the correct option for this board.**

The names were left unchanged so that the copy can be compared with the manufacturer's original.
