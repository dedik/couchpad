#pragma once

// Czasy, które decydują o tym, jak pilot się zachowuje. Zebrane w jednym miejscu,
// żeby strojenie nie wymagało szukania po komponentach. Stałe używane tylko przez
// jedną funkcję (symulacja, Wi-Fi, NTP) zostają przy niej.

#include <cstdint>

namespace core::config {

// --- łącza ---

// Przerwa między próbami połączenia z brokerem. Pierwsza próba pada, bo Wi-Fi
// wstaje kilka sekund po kliencie MQTT; domyślne 10 s trzymało ekrany w łączeniu
// długo po tym, jak sieć już była.
constexpr int kMqttReconnectMs = 1500;
// Limit na pojedynczą operację sieciową klienta MQTT.
constexpr int kMqttNetworkTimeoutMs = 6000;

// Po tylu milisekundach polecenie bez snapshotu uznajemy za niepotwierdzone.
// Źródła zgłaszają odrzucenie jawnie; ten limit zabezpiecza ciszę w eterze.
constexpr int64_t kConfirmTimeoutMs = 3000;
// Po tylu milisekundach bez snapshotu uznajemy, że druga strona nie odpowiada.
// Liczone od ostatniego kroku łączenia: samo skojarzenie z Wi-Fi potrafi zająć
// osiem sekund przy słabym sygnale.
constexpr int64_t kSyncTimeoutMs = 15000;

// --- ekran i dotyk ---

// Okno na drugie stuknięcie. Pilot Siri rozpoznaje Home ×2 w podobnym czasie.
constexpr uint32_t kDoubleTapMs = 300;
// Najkrótszy odstęp między krokami gładzika Apple TV. Każdy krok idzie przez
// Home Assistanta, więc ograniczamy je do około ośmiu na sekundę (ADR 0007).
constexpr int64_t kTrackpadStepIntervalMs = 120;
// Co ile wysyłać tekst na żywo. Krótsze okno to więcej poleceń przez Home
// Assistanta przy szybkim pisaniu, dłuższe — widoczne opóźnienie na TV.
constexpr uint32_t kKeyboardLiveIntervalMs = 250;

}  // namespace core::config
