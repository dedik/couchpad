# Bring-up płytki

## Cel

Przed implementacją funkcji biznesowych trzeba zweryfikować działanie dokładnie tej płytki: ekran, dotyk, ESP32-C6 i Wi-Fi. Dopiero wynik tej procedury pozwala przypiąć wersje ESP-IDF oraz komponentów.

## Zasady bezpieczeństwa

- Nie kasować flasha i nie wgrywać firmware'u bez zgody użytkownika.
- Nie przyjmować pinów ani timingów z innych ekranów 4,3 cala jako faktu.
- Wszystkie potwierdzone wartości wpisywać w `docs/hardware/verified-board-map.md` z datą, wersją SDK i sposobem testu.

## Kryteria akceptacji

1. Urządzenie uruchamia minimalny obraz na całej powierzchni 480 × 800.
2. Dotyk wskazuje poprawne współrzędne w każdym narożniku i po obrocie ekranu.
3. ESP32-C6 łączy się z Wi-Fi, utrzymuje połączenie i umożliwia MQTT.
4. Przykładowy ekran LVGL reaguje na dotyk bez resetów i bez widocznych artefaktów.
5. Log startowy, commit źródeł przykładu i wersje zależności są zapisane w raporcie walidacji.

Po pozytywnym teście wydzielimy kod sprzętowy w `components/board/` i zakazujemy pozostałym modułom bezpośredniego używania pinów.
