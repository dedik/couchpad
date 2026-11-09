#include "domain/app_state.hpp"

namespace domain {
namespace {

bool is_targeted(ControlId control) {
    switch (control) {
        case ControlId::LightPower:
        case ControlId::LightBrightness:
        case ControlId::LightKelvin:
        case ControlId::LightEffect:
        case ControlId::RadiatorPower:
        case ControlId::RadiatorMode:
        case ControlId::RadiatorTarget:
            return true;
        default:
            return false;
    }
}

}  // namespace

PendingEntry* AppState::find(ControlId control, const char* target) {
    for (auto& entry : pending_) {
        if (entry.control == control && entry.status != ControlStatus::Idle) {
            if (is_targeted(control) && target != nullptr && target[0] != '\0') {
                if (entry.requested == target) {
                    return &entry;
                }
            } else {
                return &entry;
            }
        }
    }
    return nullptr;
}

void AppState::mark_pending(ControlId control, const Label& requested, uint8_t value,
                            int64_t expires_at_ms, int16_t number) {
    PendingEntry* slot = find(control, requested.c_str());
    if (slot == nullptr) {
        for (auto& entry : pending_) {
            if (entry.status == ControlStatus::Idle) {
                slot = &entry;
                break;
            }
        }
    }
    if (slot == nullptr) {
        // Osiem jednocześnie oczekujących sterowań to już nie jest normalna praca.
        // Najstarsze i tak wygasa samo; nowego nie gubimy, tylko nadpisujemy pierwsze.
        slot = &pending_[0];
    }
    slot->control = control;
    slot->status = ControlStatus::Pending;
    slot->requested = requested;
    slot->requested_value = value;
    slot->requested_number = number;
    slot->expires_at_ms = expires_at_ms;
}

void AppState::mark_failed(ControlId control, int64_t clear_at_ms, const char* target) {
    PendingEntry* slot = find(control, target);
    if (slot == nullptr) {
        return;
    }
    slot->status = ControlStatus::Failed;
    slot->expires_at_ms = clear_at_ms;
}

void AppState::clear_pending(ControlId control, const char* target) {
    PendingEntry* slot = find(control, target);
    if (slot != nullptr) {
        *slot = PendingEntry{};
    }
}

void AppState::clear_all_pending() {
    for (auto& entry : pending_) {
        entry = PendingEntry{};
    }
}

bool AppState::expire(int64_t now_ms) {
    bool changed = false;
    for (auto& entry : pending_) {
        if (entry.status == ControlStatus::Idle || now_ms < entry.expires_at_ms) {
            continue;
        }
        if (entry.status == ControlStatus::Pending) {
            // Snapshot nie przyszedł w czasie — pokazujemy brak potwierdzenia,
            // ale nie przesuwamy zaznaczenia. Wybór zmienia wyłącznie snapshot.
            entry.status = ControlStatus::Failed;
            entry.expires_at_ms = now_ms + 900;
        } else {
            entry = PendingEntry{};
        }
        changed = true;
    }
    return changed;
}

ControlStatus AppState::status_of(ControlId control, const char* target) const {
    const PendingEntry* entry = pending_of(control, target);
    return entry != nullptr ? entry->status : ControlStatus::Idle;
}

const PendingEntry* AppState::pending_of(ControlId control, const char* target) const {
    for (const auto& entry : pending_) {
        if (entry.control == control && entry.status != ControlStatus::Idle) {
            if (is_targeted(control) && target != nullptr && target[0] != '\0') {
                if (entry.requested == target) {
                    return &entry;
                }
            } else {
                return &entry;
            }
        }
    }
    return nullptr;
}

}  // namespace domain
