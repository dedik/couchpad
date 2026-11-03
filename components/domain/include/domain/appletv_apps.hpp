#pragma once

// Aplikacje na kafelkach ekranu Apple TV.
//
// Kafelek łączy się z aplikacją przez bundle id, a nie przez nazwę: nazwy w `source_list`
// zależą od wersji aplikacji i języka systemu (ADR 0007). Bundle id przepisuje się
// z prawdziwego urządzenia według docs/integrations/appletv.md — nie zgaduje.
//
// Pusty bundle id oznacza, że aplikacji jeszcze nie odczytano. Kafelek zostaje wtedy
// nieaktywny, bo żaden snapshot nie poda takiej aplikacji.

#include <cstddef>

namespace domain {

struct AppleTvTile {
    const char* key;         // stały klucz kafelka w UI i symulacji
    const char* bundle_id;   // `app_id` z Home Assistanta
};

inline constexpr std::size_t kAppleTvTileCount = 3;

inline constexpr AppleTvTile kAppleTvTiles[kAppleTvTileCount] = {
    // Odczytane z Apple TV w salonie 2026-10-31 (media_player.browse_media).
    {"infuse", "com.firecore.infuse"},
    {"youtube", "com.google.ios.youtube"},
    {"wppilot", "pl.pilotwp.appletv"},
};

// Identyfikator, pod którym kafelek szuka aplikacji w snapshocie. Dopóki bundle id
// nie jest odczytany, kafelek gra pod swoim kluczem — taki zna tylko symulacja,
// więc na prawdziwym Apple TV zostaje nieaktywny.
inline const char* tile_app_id(const AppleTvTile& tile) {
    return tile.bundle_id[0] != '\0' ? tile.bundle_id : tile.key;
}

}  // namespace domain
