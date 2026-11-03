#pragma once

// Lista kanałów tunera dla menu kanałów na ekranie TV (ADR 0008).
//
// Publikuje ją bridge Home Assistanta na osobnym, retained temacie `channels`
// kontraktu v1. Nie należy do `DeviceSnapshot`: ma kilka kilobajtów, a snapshot
// bywa kopiowany na ciasnym stosie zadania UI. Lista żyje w jednym egzemplarzu
// w źródle snapshotów i interfejs czyta ją przez kontroler.

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/text.hpp"
#include "domain/snapshot.hpp"

namespace domain {

inline constexpr std::size_t kMaxChannels = 96;

struct Channel {
    core::Text<8> number;  // numer programu tak, jak podaje tuner
    Label name;
    // Identyfikator z parametrami multipleksu. Nim przełączamy, bo nazwy się
    // powtarzają (TVP World pod 33 i 91).
    core::Text<40> id;
};

struct ChannelList {
    std::array<Channel, kMaxChannels> items{};
    std::size_t count = 0;
    // Rośnie z każdą odebraną listą, żeby ekran przepisywał wiersze tylko po zmianie.
    uint32_t revision = 0;
};

}  // namespace domain
