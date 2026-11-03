#include "application/app_controller.hpp"

#include "core/clock.hpp"
#include "core/config.hpp"
#include "esp_log.h"

namespace application {
namespace {

constexpr char kTag[] = "app";

using core::config::kConfirmTimeoutMs;
using core::config::kSyncTimeoutMs;

bool is_momentary(const domain::UiIntent& intent) {
    return intent.kind == domain::IntentKind::Navigate ||
           intent.kind == domain::IntentKind::Momentary;
}

}  // namespace

void AppController::start() {
    source_->start();
}

void AppController::set_source(SnapshotSource& source) {
    source_ = &source;
    state_ = domain::AppState{};
    connecting_since_ms_ = core::now_ms();
    source_->start();
    notify();
}

void AppController::notify() {
    if (on_change_ != nullptr) {
        on_change_(on_change_context_);
    }
}

void AppController::dispatch(const domain::UiIntent& intent) {
    if (intent.kind == domain::IntentKind::RequestSync) {
        // ADR 0005: do czasu nowego snapshotu nie wolno pokazywać stanu z pamięci,
        // więc żądanie odświeżenia wraca na nakładkę łączenia. Zdejmie ją dopiero
        // snapshot o nowej rewizji albo limit czasu.
        state_.clear_all_pending();
        state_.set_phase(domain::LinkPhase::Connecting);
        state_.set_step(domain::LinkStep::Sync);
        connecting_since_ms_ = core::now_ms();
        source_->request_sync();
        notify();
        return;
    }

    source_->send(intent);

    // Polecenia chwilowe nie mają stanu do potwierdzenia: nawigacja, skrót, scena.
    if (!is_momentary(intent) && intent.control != domain::ControlId::None) {
        state_.mark_pending(intent.control, intent.text, intent.value,
                            core::now_ms() + kConfirmTimeoutMs, intent.number);
        notify();
    }
}

void AppController::pump() {
    bool changed = false;

    SourceEvent& event = event_;
    while (source_->poll(event)) {
        switch (event.kind) {
            case SourceEvent::Kind::Snapshot: {
                // Snapshot o rewizji nie większej niż przetworzona jest ignorowany.
                if (event.snapshot.revision <= state_.snapshot().revision) {
                    break;
                }
                state_.snapshot() = event.snapshot;
                // Nowy obraz stanu zamyka wszystkie oczekiwania: to on, a nie
                // dotknięcie, przesuwa zaznaczenie.
                state_.clear_all_pending();
                // I on zdejmuje nakładkę — świeży stan jest jedyną definicją
                // gotowości, niezależnie od tego, jakie źródło go podało i czy
                // przedtem staliśmy na łączeniu, czy na błędzie. Bez tego ekran
                // błędu zostawał na swoim, choć łącze już wróciło.
                if (state_.phase() != domain::LinkPhase::Ready) {
                    // Czas od wejścia w łączenie (start albo wybudzenie) do
                    // pierwszego świeżego stanu — to on decyduje, jak szybko
                    // pilot „się włącza”.
                    if (connecting_since_ms_ != 0) {
                        ESP_LOGI(kTag, "stan gotowy po %lld ms",
                                 static_cast<long long>(core::now_ms() - connecting_since_ms_));
                    }
                    state_.set_phase(domain::LinkPhase::Ready);
                    connecting_since_ms_ = 0;
                }
                changed = true;
                break;
            }
            case SourceEvent::Kind::Phase: {
                // Zegar liczy od ostatniego kroku łączenia, nie od startu
                // urządzenia — inaczej na słabym Wi-Fi limit wyczerpywał się,
                // zanim pilot zdążył dojść do brokera. Powtórzony ten sam krok
                // go nie przesuwa, bo pętla ponawiania odsuwałaby błąd bez końca.
                if (event.phase == domain::LinkPhase::Connecting &&
                    (state_.phase() != domain::LinkPhase::Connecting ||
                     state_.step() != event.step)) {
                    connecting_since_ms_ = core::now_ms();
                }
                state_.set_phase(event.phase);
                state_.set_step(event.step);
                if (event.phase != domain::LinkPhase::Ready) {
                    state_.clear_all_pending();
                }
                changed = true;
                break;
            }
            case SourceEvent::Kind::Rejected: {
                state_.mark_failed(event.control, core::now_ms() + 900);
                changed = true;
                break;
            }
        }
    }

    if (state_.expire(core::now_ms())) {
        changed = true;
    }

    if (state_.phase() == domain::LinkPhase::Connecting && connecting_since_ms_ != 0 &&
        core::now_ms() - connecting_since_ms_ > kSyncTimeoutMs) {
        state_.set_phase(domain::LinkPhase::Error);
        connecting_since_ms_ = 0;
        changed = true;
    }

    if (changed) {
        notify();
    }
}

}  // namespace application
