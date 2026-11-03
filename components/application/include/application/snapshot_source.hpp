#pragma once

// Szew między aplikacją a światem. Dziś po drugiej stronie stoi symulacja,
// docelowo klient MQTT — interfejs się nie zmienia.
//
// Reguła, która ten kształt wymusza: dotknięcie nie może czekać na sieć.
// `send()` tylko wkłada polecenie do kolejki, a odpowiedzi odbiera `poll()`
// wołane z zadania UI.

#include "domain/channels.hpp"
#include "domain/app_state.hpp"
#include "domain/intent.hpp"
#include "domain/snapshot.hpp"
#include "domain/weather.hpp"

namespace application {

struct SourceEvent {
    enum class Kind : uint8_t {
        Snapshot,   // nowy, pełny obraz stanu
        Phase,      // zmiana fazy łącza (nakładka wybudzania)
        Rejected,   // polecenie nie zostało potwierdzone
    };

    Kind kind = Kind::Phase;
    domain::DeviceSnapshot snapshot;
    domain::LinkPhase phase = domain::LinkPhase::Connecting;
    domain::LinkStep step = domain::LinkStep::Wifi;
    domain::ControlId control = domain::ControlId::None;
};

class SnapshotSource {
public:
    virtual ~SnapshotSource() = default;

    // Uruchamia zadanie robocze źródła.
    virtual void start() = 0;

    // Wkłada polecenie do kolejki. Nigdy nie blokuje zadania UI.
    virtual void send(const domain::UiIntent& intent) = 0;

    // Wymusza nowy snapshot (odpowiednik `remote.sync.request`).
    virtual void request_sync() = 0;

    // Odbiera jedno zdarzenie, jeśli czeka. Wołane wyłącznie z zadania UI.
    virtual bool poll(SourceEvent& out) = 0;

    // Lista kanałów tunera (ADR 0008) albo nullptr, gdy źródło jej nie zna.
    // Zmiana listy przychodzi razem z nowym snapshotem.
    virtual const domain::ChannelList* channels() const { return nullptr; }

    // Ostatnia pogoda (ADR 0011) albo nullptr. Treść, nie stan: zostaje po wybudzeniu.
    virtual const domain::Weather* weather() const { return nullptr; }
};

}  // namespace application
