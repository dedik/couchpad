#include "application/network_controller.hpp"

#include "core/clock.hpp"
#include "transport/time_sync.hpp"

namespace application {
namespace {

// Odstęp między próbami powrotu na zapamiętaną sieć.
constexpr int64_t kRetryEveryMs = 15000;

}  // namespace

void NetworkController::start() {
    storage::init();

    storage::WifiCredentials saved;
    const bool has_saved = storage::load_wifi(saved);

    wifi_.start();

    if (has_saved) {
        remembered_.assign(saved.ssid.c_str());
        // Dotknięcie nie czeka na sieć, a start tym bardziej: zlecenie idzie
        // do kolejki i usługa zajmie się nim we własnym zadaniu.
        transport::Ssid ssid;
        transport::Password password;
        ssid.assign(saved.ssid.c_str());
        password.assign(saved.password.c_str());
        next_retry_ms_ = core::now_ms() + kRetryEveryMs;
        wifi_.connect(ssid, password);
    }
}

void NetworkController::retry_remembered() {
    storage::WifiCredentials saved;
    if (!storage::load_wifi(saved)) {
        remembered_.clear();
        return;
    }
    transport::Ssid ssid;
    transport::Password password;
    ssid.assign(saved.ssid.c_str());
    password.assign(saved.password.c_str());
    next_retry_ms_ = core::now_ms() + kRetryEveryMs;
    wifi_.connect(ssid, password);
}

void NetworkController::scan() {
    // Jeśli skan zerwie łącze, chcemy wrócić natychmiast, a nie po odstępie
    // przewidzianym na wyłączony router.
    next_retry_ms_ = 0;
    wifi_.scan();
}

void NetworkController::connect(const transport::Ssid& ssid, const transport::Password& password) {
    pending_ssid_ = ssid;
    pending_password_ = password;
    awaiting_save_ = true;
    wifi_.connect(ssid, password);
}

void NetworkController::forget() {
    storage::clear_wifi();
    remembered_.clear();
    awaiting_save_ = false;
    wifi_.disconnect();
}

void NetworkController::pump() {
    bool changed = false;

    transport::WifiEvent event;
    while (wifi_.poll(event)) {
        changed = true;
        if (event.kind != transport::WifiEvent::Kind::Phase) {
            continue;
        }
        // Sam skan nie jest utratą łącza — liczy się dopiero zejście z „połączono”.
        if (last_phase_ == transport::WifiPhase::Connected &&
            (event.phase == transport::WifiPhase::Idle ||
             event.phase == transport::WifiPhase::Failed ||
             event.phase == transport::WifiPhase::Connecting)) {
            ++drops_;
        }
        if (event.phase == transport::WifiPhase::Failed) {
            ++failures_;
        }
        last_phase_ = event.phase;
        if (event.phase == transport::WifiPhase::Connected) {
            next_retry_ms_ = 0;
            // Pilot nie ma podtrzymywanego zegara — godzina przychodzi z sieci.
            transport::start_time_sync();
        }
        if (event.phase == transport::WifiPhase::Connected && awaiting_save_) {
            // Zapisujemy dopiero po potwierdzonym połączeniu.
            storage::WifiCredentials credentials;
            credentials.ssid.assign(pending_ssid_.c_str());
            credentials.password.assign(pending_password_.c_str());
            storage::save_wifi(credentials);
            remembered_ = pending_ssid_;
            awaiting_save_ = false;
            pending_password_.clear();
        } else if (event.phase == transport::WifiPhase::Failed) {
            awaiting_save_ = false;
            pending_password_.clear();
        }
    }

    // Klient czasu zatrzymuje się sam, gdy tylko czas przyjdzie.
    transport::service_time_sync();

    // Zerwane łącze wraca samo, o ile użytkownik nie kazał zapomnieć sieci.
    // Nieudana próba też: przy słabym sygnale pierwsze łączenie po starcie
    // potrafi paść (powód 2, brak odpowiedzi na uwierzytelnienie), a bez
    // ponowienia pilot zostawał bez sieci, aż ktoś wpisał hasło od nowa.
    const auto phase = wifi_.phase();
    const bool idle = phase == transport::WifiPhase::Idle || phase == transport::WifiPhase::Failed;
    if (idle && !remembered_.empty() && !awaiting_save_ && core::now_ms() >= next_retry_ms_) {
        retry_remembered();
    }

    if (changed && on_change_ != nullptr) {
        on_change_(on_change_context_);
    }
}

}  // namespace application
