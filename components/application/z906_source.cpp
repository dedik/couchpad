#include "application/z906_source.hpp"

#include "storage/settings.hpp"

namespace application {

void Z906Source::start(transport::MqttBus& bus, const storage::MqttSettings& settings,
                       bool loaded) {
    configured_ = loaded && settings.complete();
    if (!configured_) {
        // Bez adresu nie ma do czego się łączyć. Interfejs ma o tym powiedzieć
        // wprost, zamiast w nieskończoność pokazywać nakładkę łączenia.
        report_missing_settings_ = true;
        return;
    }
    if (!client_.start(bus, settings)) {
        report_missing_settings_ = true;
    }
}

void Z906Source::stop() {
    client_.stop();
}

void Z906Source::send(const domain::UiIntent& intent) {
    client_.publish_intent(intent);
}

void Z906Source::request_sync() {
    client_.request_sync();
}

bool Z906Source::poll(SourceEvent& out) {
    if (report_missing_settings_) {
        report_missing_settings_ = false;
        out.control = domain::ControlId::None;
        out.kind = SourceEvent::Kind::Phase;
        out.phase = domain::LinkPhase::Error;
        out.step = domain::LinkStep::Mqtt;
        return true;
    }

    transport::Z906Event& event = event_;
    if (!client_.poll(event)) {
        return false;
    }

    out.control = domain::ControlId::None;
    if (event.kind == transport::Z906Event::Kind::Snapshot) {
        out.kind = SourceEvent::Kind::Snapshot;
        out.snapshot.revision = event.revision;
        out.snapshot.z906 = event.z906;
    } else {
        out.kind = SourceEvent::Kind::Phase;
        out.phase = event.phase;
        out.step = event.step;
    }
    return true;
}

}  // namespace application
