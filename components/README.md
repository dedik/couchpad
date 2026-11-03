# Komponenty

[English](README.en.md)

## Obsługa płytki, przejęta od producenta

Dwa katalogi pochodzą z przykładu Guition dla modelu `JC4880P443C_I_W` i są tu **celowo**, mimo że
wyglądają na zewnętrzne zależności:

| Katalog | Co to jest |
| --- | --- |
| `board_bsp/` | BSP Espressifu dla ESP32-P4 Function EV Board, przerobiony przez producenta pod tę płytkę |
| `esp_lcd_st7701_patched/` | sterownik ST7701 z **własną sekwencją inicjalizacji** producenta |

**Nie zastępuj ich wersjami z rejestru komponentów.** Standardowy `esp_lcd_st7701` ma inną sekwencję
inicjalizacji i daje na tym panelu czarny ekran. Potwierdzają to zarówno przykład producenta, jak
i niezależna implementacja w ESPHome dla tego modelu.

### Zmiana wprowadzona przez nas

W `board_bsp/esp32_p4_function_ev_board.c` usunięte jest ustawienie źródła zegara PLL dla PHY
w MIPI-DSI. Oryginał podawał tam stałą zgodności oznaczającą `PLL_F20M`, co na układach od rewizji
3.0 wzwyż kończy się `abort()` i pętlą restartów jeszcze przed zapaleniem ekranu. Powód i pełna
diagnoza są w komentarzu w kodzie oraz w [`../docs/hardware/verified-board-map.md`](../docs/hardware/verified-board-map.md).

### Mylące napisy w kodzie producenta

- `Install EK79007 LCD control panel` — nieprawda, ten blok woła `esp_lcd_new_panel_st7701()`.
- Makro `ST7701_480_360_PANEL_60HZ_DPI_CONFIG` ma w nazwie 480 × 360, ale jego treść została
  przez producenta przepisana na 480 × 800.
- Opcja Kconfig `BSP_LCD_TYPE_1024_600` opisana jako „LCD 7-inch 1024x600 - ek79007" wybiera
  w tej kopii gałąź ST7701 480 × 800. **To jest właściwa opcja dla tej płytki.**

Nazwy zostały bez zmian, żeby dało się porównać kopię z oryginałem producenta.
