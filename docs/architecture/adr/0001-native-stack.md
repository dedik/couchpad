# ADR 0001: Natywny stos UI

- **Status:** zaakceptowane
- **Data:** 2026-10-29

## Decyzja

Firmware użyje ESP-IDF, C++17 i LVGL 9. ESP-IDF jest natywnym frameworkiem ESP32-P4; LVGL renderuje bez silnika przeglądarki.

## Konsekwencje

- Otrzymujemy małe opóźnienia dotyku, pełną kontrolę nad pamięcią i MIPI-DSI oraz prostą ścieżkę diagnostyczną.
- UI będzie wymagało własnych komponentów i stylu, ale daje to spójny, szybki produkt.
- ESPHome, web UI, React, Flutter i podobne warstwy nie są częścią firmware'u pilota v1.

## Wersje

Dokładny tag ESP-IDF i zależności LVGL zostanie zapisany po przejściu procedury bring-up. Źródłem konfiguracji bazowej jest przykład producenta dla dokładnego modelu, nie przykład podobnej płytki.
