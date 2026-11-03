# ADR 0003: Jedno zadanie jest właścicielem LVGL

[English](0003-single-ui-owner.en.md)

- **Status:** zaakceptowane
- **Data:** 2026-10-29

## Decyzja

Wszystkie operacje na `lv_obj_t` i renderowanie wykonuje jedno zadanie UI. Zadania MQTT, Wi-Fi i storage komunikują się z nim kolejką zdarzeń oraz nie dotykają obiektów LVGL.

## Uzasadnienie

LVGL nie daje bezpiecznego modelu równoczesnej modyfikacji widoków z wielu zadań. Jedna ścieżka aktualizacji eliminuje sporadyczne błędy dotyku i artefakty ekranu.

## Konsekwencje

- Sieć nie może blokować interakcji.
- Debugowanie jest prostsze: każda zmiana widoku ma źródło w `UiIntent` lub `DeviceSnapshot`.
