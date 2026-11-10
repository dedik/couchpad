#pragma once

// Wejścia telewizora na zakładce Pilot (ADR 0020): telewizja i wejścia nazwane
// w ustawieniach TV (Apple TV, PC). Wejście z domyślną nazwą („HDMI 2”) zwykle nie jest
// używane, więc zostaje na pełnym ekranie TV — chyba że jest aktywne. Aktywne zawsze
// stoi pierwsze. Ta sama reguła co w aplikacji na Androida (`remoteSources`).

#include <cctype>
#include <cstddef>
#include <cstring>

#include "domain/snapshot.hpp"

namespace domain {

inline constexpr char kLiveTvId[] = "livetv";

// „HDMI 2”, „hdmi2”, „HDMI  4” — nazwa, jaką telewizor nadaje sam.
inline bool default_input_name(const char* label) {
    if (label == nullptr) {
        return false;
    }
    const char* word = "hdmi";
    std::size_t i = 0;
    for (; word[i] != '\0'; ++i) {
        if (std::tolower(static_cast<unsigned char>(label[i])) != word[i]) {
            return false;
        }
    }
    while (label[i] == ' ') {
        ++i;
    }
    if (!std::isdigit(static_cast<unsigned char>(label[i]))) {
        return false;
    }
    while (std::isdigit(static_cast<unsigned char>(label[i]))) {
        ++i;
    }
    return label[i] == '\0';
}

// Indeksy w `source_options` w kolejności do pokazania.
struct RemoteSources {
    std::size_t index[kMaxOptions]{};
    std::size_t count = 0;
};

inline bool is_live_tv(const TvState& tv, std::size_t option) {
    return option < tv.source_ids.size() && std::strcmp(tv.source_ids[option].c_str(), kLiveTvId) == 0;
}

inline RemoteSources remote_sources(const TvState& tv) {
    RemoteSources out;
    // Najpierw aktywne, potem pozostałe w kolejności z telewizora.
    for (int pass = 0; pass < 2; ++pass) {
        for (std::size_t i = 0; i < tv.source_options.size() && out.count < kMaxOptions; ++i) {
            const bool active = !tv.source.empty() && tv.source_options[i] == tv.source;
            if ((pass == 0) != active) {
                continue;
            }
            if (!active && !is_live_tv(tv, i) && default_input_name(tv.source_options[i].c_str())) {
                continue;
            }
            out.index[out.count++] = i;
        }
    }
    return out;
}

}  // namespace domain
