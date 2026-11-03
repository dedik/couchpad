#pragma once

// Log pilota przez MQTT — narzędzie na czas rozwoju (CONFIG_PILOT_REMOTE_LOG).
//
// Każda linia ESP_LOG trafia jak dotąd na port szeregowy, a kopia idzie do
// tematu `pilot_debug/<id>/log` na tym samym brokerze. Linie sprzed połączenia
// czekają w buforze w PSRAM, więc w temacie widać też start urządzenia.
//
// Log nie zawiera sekretów: nazwy sieci, hasła i adresy nie są logowane
// (AGENTS.md), a tag ESP-Hosted, który wypisywał nazwę sieci, jest wyciszony.

#include "storage/settings.hpp"

namespace transport::log_sink {

// Przechwytuje log od tej chwili. Wołać jak najwcześniej w app_main.
void capture();

// Łączy się z brokerem i zaczyna wysyłać. `id` to identyfikator pilota w temacie.
void start(const storage::MqttSettings& settings, const char* id);

}  // namespace transport::log_sink
