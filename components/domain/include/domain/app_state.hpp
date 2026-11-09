#pragma once

// Pełny stan, z którego rysuje się interfejs: ostatni potwierdzony snapshot,
// faza łącza oraz sterowania czekające na potwierdzenie.
//
// ADR 0005: do czasu nowego snapshotu po wybudzeniu nie pokazujemy stanu z pamięci.

#include <array>
#include <cstddef>
#include <cstdint>

#include "domain/intent.hpp"
#include "domain/snapshot.hpp"

namespace domain {

// Faza łącza z domem. Interaktywne ekrany są dostępne wyłącznie w `Ready`.
enum class LinkPhase : uint8_t { Connecting, Ready, Error };

// Kroki pokazywane na nakładce wybudzania.
enum class LinkStep : uint8_t { Wifi = 0, Mqtt = 1, Sync = 2 };

enum class ControlStatus : uint8_t {
    Idle,       // spoczynek
    Pending,    // polecenie wysłane, czekamy na snapshot
    Failed,     // snapshot nie potwierdził zmiany w czasie
};

// Jedno sterowanie w trakcie potwierdzania. Zapamiętujemy, o co poproszono,
// żeby podświetlić dokładnie tę opcję, a nie całą grupę.
struct PendingEntry {
    ControlId control = ControlId::None;
    ControlStatus status = ControlStatus::Idle;
    Label requested;
    uint8_t requested_value = 0;
    int16_t requested_number = 0;  // temperatura klimatyzacji w dziesiątych
    int64_t expires_at_ms = 0;
};

inline constexpr std::size_t kMaxPending = 8;

class AppState {
public:
    const DeviceSnapshot& snapshot() const { return snapshot_; }
    DeviceSnapshot& snapshot() { return snapshot_; }

    LinkPhase phase() const { return phase_; }
    LinkStep step() const { return step_; }
    void set_phase(LinkPhase phase) { phase_ = phase; }
    void set_step(LinkStep step) { step_ = step; }

    // `true`, gdy wolno pokazać sterowanie danego urządzenia jako aktualne.
    bool tv_fresh() const { return phase_ == LinkPhase::Ready && snapshot_.tv.available; }
    bool z906_fresh() const { return phase_ == LinkPhase::Ready && snapshot_.z906.available; }
    bool appletv_fresh() const { return phase_ == LinkPhase::Ready && snapshot_.appletv.available; }
    bool climate_fresh() const { return phase_ == LinkPhase::Ready && snapshot_.climate.available; }
    bool lights_fresh() const { return phase_ == LinkPhase::Ready && snapshot_.lights.available; }
    bool radiators_fresh() const { return phase_ == LinkPhase::Ready && snapshot_.radiators.available; }

    // Rejestracja i wygasanie oczekiwania.
    void mark_pending(ControlId control, const Label& requested, uint8_t value, int64_t expires_at_ms,
                      int16_t number = 0);
    void mark_failed(ControlId control, int64_t clear_at_ms, const char* target = nullptr);
    void clear_pending(ControlId control, const char* target = nullptr);
    void clear_all_pending();
    // Przenosi przeterminowane wpisy do `Failed` i kasuje wygasłe `Failed`.
    // Zwraca `true`, gdy coś się zmieniło i trzeba przerysować.
    bool expire(int64_t now_ms);

    ControlStatus status_of(ControlId control, const char* target = nullptr) const;
    const PendingEntry* pending_of(ControlId control, const char* target = nullptr) const;

private:
    PendingEntry* find(ControlId control, const char* target = nullptr);

    DeviceSnapshot snapshot_;
    LinkPhase phase_ = LinkPhase::Connecting;
    LinkStep step_ = LinkStep::Wifi;
    std::array<PendingEntry, kMaxPending> pending_{};
};

}  // namespace domain
