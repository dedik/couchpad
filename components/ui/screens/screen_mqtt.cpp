#include <cstdio>
#include <cstdlib>

#include "ui_internal.hpp"

namespace ui {
namespace {

// Ustawienia brokera i kontrolera Z906. Adres domowej infrastruktury wpisuje
// się tutaj, na urządzeniu, i nigdzie poza NVS nie trafia.

void on_back(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::show_page(context.mqtt.page, false);
}

void save_and_reconnect(Context& context) {
    storage::save_mqtt(context.mqtt.draft);
    // Ponowne wejście w to samo źródło restartuje klientów z nowym adresem.
    context.controller->set_source(*context.live);
    render(context);
}

void on_apply(void* raw) {
    save_and_reconnect(*static_cast<Context*>(raw));
}

void on_host_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.mqtt.draft.host.assign(value);
    render(context);
}

void on_port_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    const int port = std::atoi(value);
    context.mqtt.draft.port = static_cast<uint16_t>(port > 0 && port < 65536 ? port : 1883);
    render(context);
}

void on_username_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.mqtt.draft.username.assign(value);
    render(context);
}

void on_password_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.mqtt.draft.password.assign(value);
    render(context);
}

void on_z906_id_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.mqtt.draft.z906_id.assign(value);
    render(context);
}

void on_remote_id_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.mqtt.draft.remote_id.assign(value);
    render(context);
}

void on_tv_prefix_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    context.mqtt.draft.tv_prefix.assign(value);
    render(context);
}

void on_host(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::open_text_editor(*context.editor, "ADRES BROKERA", "Host albo adres IP",
                              context.mqtt.draft.host.c_str(), widgets::EditorMode::Text,
                              &on_host_ready, &context);
}

void on_port(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    char value[8];
    std::snprintf(value, sizeof(value), "%u", static_cast<unsigned>(context.mqtt.draft.port));
    widgets::open_text_editor(*context.editor, "PORT", "Zwykle 1883", value,
                              widgets::EditorMode::Number, &on_port_ready, &context);
}

void on_username(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::open_text_editor(*context.editor, "UŻYTKOWNIK", "Nazwa użytkownika brokera",
                              context.mqtt.draft.username.c_str(), widgets::EditorMode::Text,
                              &on_username_ready, &context);
}

void on_password(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::open_text_editor(*context.editor, "HASŁO", "Hasło do brokera", "",
                              widgets::EditorMode::Secret, &on_password_ready, &context);
}

void on_z906_id(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::open_text_editor(*context.editor, "IDENTYFIKATOR Z906",
                              "Postaci z906_<mac>",
                              context.mqtt.draft.z906_id.c_str(), widgets::EditorMode::Text,
                              &on_z906_id_ready, &context);
}

void on_remote_id(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::open_text_editor(*context.editor, "IDENTYFIKATOR PILOTA",
                              "Ten sam co w pakiecie Home Assistanta",
                              context.mqtt.draft.remote_id.c_str(), widgets::EditorMode::Text,
                              &on_remote_id_ready, &context);
}

void on_tv_prefix(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::open_text_editor(*context.editor, "PREFIKS TEMATÓW TV",
                              "Z zakładki MQTT w Glasshouse, zwykle lgtv",
                              context.mqtt.draft.tv_prefix.c_str(), widgets::EditorMode::Text,
                              &on_tv_prefix_ready, &context);
}

void set_row(const widgets::ListRow& row, const char* value, bool filled) {
    lv_label_set_text(row.value, value);
    lv_obj_set_style_text_color(row.value, filled ? theme::text() : theme::faint(), LV_PART_MAIN);
}

}  // namespace

void build_mqtt(Context& context) {
    MqttScreen& screen = context.mqtt;

    storage::load_mqtt(screen.draft);

    screen.page = widgets::make_page("MQTT", &on_back, &context);
    screen.status = widgets::make_readout(screen.page.body, "Stan");

    screen.host = widgets::make_list_row(screen.page.body, LV_SYMBOL_WIFI, "Adres brokera", "", "",
                                         &on_host, &context);
    screen.port = widgets::make_list_row(screen.page.body, LV_SYMBOL_SETTINGS, "Port", "", "",
                                         &on_port, &context);
    screen.username = widgets::make_list_row(screen.page.body, LV_SYMBOL_LIST, "Użytkownik", "",
                                             "", &on_username, &context);
    screen.password = widgets::make_list_row(screen.page.body, LV_SYMBOL_CLOSE, "Hasło", "", "",
                                             &on_password, &context);
    screen.z906_id = widgets::make_list_row(screen.page.body, LV_SYMBOL_FILE,
                                              "Kontroler Z906", "identyfikator z tematów brokera", "",
                                              &on_z906_id, &context);
    screen.remote_id = widgets::make_list_row(screen.page.body, LV_SYMBOL_HOME,
                                              "Identyfikator pilota",
                                              "Home Assistant: Apple TV", "", &on_remote_id,
                                              &context);

    screen.tv_prefix = widgets::make_list_row(screen.page.body, LV_SYMBOL_VIDEO, "Telewizor",
                                              "prefiks tematów Glasshouse", "", &on_tv_prefix,
                                              &context);

    screen.apply = widgets::make_action(screen.page.body, LV_SYMBOL_OK, "Zapisz i połącz", true,
                                        widgets::ActionLayout::Block, &on_apply, &context);
}

void open_mqtt_page(Context& context) {
    storage::load_mqtt(context.mqtt.draft);
    widgets::show_page(context.mqtt.page, true);
    render(context);
}

void render_mqtt(Context& context) {
    MqttScreen& screen = context.mqtt;
    const storage::MqttSettings& draft = screen.draft;
    set_row(screen.host, draft.host.empty() ? "nie podano" : draft.host.c_str(),
            !draft.host.empty());
    char port[8];
    std::snprintf(port, sizeof(port), "%u", static_cast<unsigned>(draft.port));
    set_row(screen.port, port, true);
    set_row(screen.username, draft.username.empty() ? "brak" : draft.username.c_str(),
            !draft.username.empty());
    // Hasła nie pokazujemy nawet w skrócie — tylko to, czy jest ustawione.
    set_row(screen.password, draft.password.empty() ? "nie ustawiono" : "zapisane",
            !draft.password.empty());
    set_row(screen.z906_id, draft.z906_id.empty() ? "nie podano" : draft.z906_id.c_str(),
            !draft.z906_id.empty());
    // Bez identyfikatora pilot nie rozmawia z Home Assistantem, a Apple TV
    // zostaje niedostępne. Dźwięk działa niezależnie.
    set_row(screen.remote_id, draft.remote_id.empty() ? "nie podano" : draft.remote_id.c_str(),
            !draft.remote_id.empty());

    set_row(screen.tv_prefix, draft.tv_prefix.empty() ? "wyłączony" : draft.tv_prefix.c_str(),
            !draft.tv_prefix.empty());

    const auto phase = context.controller->state().phase();
    const char* text = "brak połączenia";
    bool good = false;
    if (!draft.complete()) {
        text = "uzupełnij adres i identyfikator";
    } else if (phase == domain::LinkPhase::Ready) {
        text = "stan odebrany";
        good = true;
    } else if (phase == domain::LinkPhase::Connecting) {
        text = "łączenie…";
    } else {
        switch (context.live->error()) {
            case transport::MqttError::NotAuthorized:
                text = "broker odrzucił logowanie";
                break;
            case transport::MqttError::Unreachable:
                text = "broker nie odpowiada";
                break;
            case transport::MqttError::Rejected:
                text = "broker odrzucił identyfikator";
                break;
            default:
                break;
        }
    }
    screen.status.set(text, good);
}

}  // namespace ui
