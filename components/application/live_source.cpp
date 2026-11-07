#include "application/live_source.hpp"

#include "storage/settings.hpp"

namespace application {

void LiveSource::start() {
    z906_fresh_ = false;
    appletv_ = domain::AppleTvState{};
    // Bez kopii pustej struktury przez stos: wystarczy, że klima nie jest świeża.
    climate_.available = false;
    lights_.available = false;
    tv_state_ = domain::TvState{};
    stop();

    // Jedno połączenie, trzech klientów: każdy podpina się przed startem, żeby nie
    // przegapić zdarzenia połączenia, w którym subskrybuje swoje tematy.
    storage::MqttSettings settings;
    const bool loaded = storage::load_mqtt(settings);
    if (loaded) {
        bus_.open(settings);
    }
    z906_.start(bus_, settings, loaded);
    if (loaded) {
        ha_.start(bus_, settings);
        tv_.start(bus_, settings);
    }
    // Połączenie rusza, gdy jest adres brokera — także bez Z906, dla samego HA i TV.
    if (bus_.handle() != nullptr && !bus_.start() && z906_.configured()) {
        z906_.report_failure();
    }
}

void LiveSource::stop() {
    // Klienci najpierw zapominają uchwyt, dopiero potem połączenie gaśnie.
    z906_.stop();
    ha_.stop();
    tv_.stop();
    bus_.close();
}

void LiveSource::send(const domain::UiIntent& intent) {
    // Klimatyzacja i światła idą kontraktem v1, jak Apple TV (ADR 0009, 0014).
    if (domain::targets_home_assistant(intent) || domain::targets_climate(intent) ||
        domain::targets_lights(intent)) {
        ha_.publish_intent(intent);
    } else if (domain::targets_tv(intent)) {
        tv_.publish_intent(intent);
    } else {
        z906_.send(intent);
    }
}

void LiveSource::request_sync() {
    // Po wybudzeniu żadna połowa nie jest świeża, dopóki nie odpowie na nowo.
    z906_fresh_ = false;
    appletv_ = domain::AppleTvState{};
    climate_.available = false;
    lights_.available = false;
    tv_state_ = domain::TvState{};
    z906_.request_sync();
    ha_.request_sync();
    tv_.request_sync();
}

void LiveSource::merge_into(SourceEvent& out) {
    out.kind = SourceEvent::Kind::Snapshot;
    out.snapshot.revision = ++revision_;
    out.snapshot.tv = tv_state_;
    out.snapshot.z906 = z906_state_;
    out.snapshot.appletv = appletv_;
    out.snapshot.climate = climate_;
    out.snapshot.lights = lights_;
}

// Wołane z zadania LVGL, którego stos ma 8 kB. Snapshot waży około 2,5 kB, a nad
// tą funkcją `pump()` i `Z906Source::poll()` trzymają już swoje kopie — dlatego
// nic dużego nie leży tu na stosie: zdarzenie HA jest polem, a wynik powstaje
// wprost w `out`.
bool LiveSource::poll(SourceEvent& out) {
    // Pogoda nie zmienia stanu urządzeń; ekran startowy czyta ją sam co sekundę.
    ha_.take_weather(weather_);

    // Nowa lista kanałów nie zmienia stanu urządzeń, ale ekran musi się przerysować.
    if (ha_.take_channels(channels_) && z906_fresh_) {
        merge_into(out);
        return true;
    }

    while (tv_.poll(tv_event_)) {
        // Utrata brokera to brak świeżego stanu, nie ostatni znany stan.
        if (tv_event_.kind == transport::TvEvent::Kind::Snapshot) {
            tv_state_ = tv_event_.tv;
        } else {
            tv_state_ = domain::TvState{};
        }
        if (z906_fresh_) {
            merge_into(out);
            return true;
        }
    }

    while (ha_.poll(ha_event_)) {
        // Utrata brokera to brak świeżego stanu, nie ostatni znany stan.
        if (ha_event_.kind == transport::HaEvent::Kind::Snapshot) {
            appletv_ = ha_event_.appletv;
            climate_ = ha_event_.climate;
            lights_ = ha_event_.lights;
        } else {
            appletv_ = domain::AppleTvState{};
            climate_.available = false;
            lights_.available = false;
        }
        if (z906_fresh_) {
            merge_into(out);
            return true;
        }
    }

    if (!z906_.poll(out)) {
        return false;
    }
    if (out.kind == SourceEvent::Kind::Snapshot) {
        z906_state_ = out.snapshot.z906;
        z906_fresh_ = true;
        merge_into(out);
        return true;
    }
    if (out.kind == SourceEvent::Kind::Phase && out.phase != domain::LinkPhase::Ready) {
        // Każdy snapshot zdejmuje nakładkę, więc stan Apple TV nie może wyjść,
        // póki Z906 łączy się od nowa albo stoi na błędzie.
        z906_fresh_ = false;
    }
    return true;
}

}  // namespace application
