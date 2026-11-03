#include <cstdio>
#include <cstring>

#include "ui_internal.hpp"

namespace ui {
namespace {

const char* phase_text(transport::WifiPhase phase, transport::WifiError error) {
    switch (phase) {
        case transport::WifiPhase::Off: return "radio wyłączone";
        case transport::WifiPhase::Idle: return "brak połączenia";
        case transport::WifiPhase::Scanning: return "szukam sieci…";
        case transport::WifiPhase::Connecting: return "łączenie…";
        case transport::WifiPhase::Connected: return "połączono";
        case transport::WifiPhase::Failed:
            switch (error) {
                case transport::WifiError::BadPassword: return "błędne hasło";
                case transport::WifiError::NotFound: return "nie znaleziono sieci";
                case transport::WifiError::NoAddress: return "brak adresu";
                default: return "nie udało się połączyć";
            }
    }
    return "";
}

// Cztery kreski zasięgu wystarczą; dokładna wartość w dBm nic nie mówi użytkownikowi.
const char* signal_text(int8_t rssi) {
    if (rssi >= -55) return "silny";
    if (rssi >= -67) return "dobry";
    if (rssi >= -78) return "słaby";
    return "bardzo słaby";
}

// Najczęstsze powody z wifi_err_reason_t, po ludzku. Kod zostaje obok, bo
// to on prowadzi do dokumentacji ESP-IDF.
const char* reason_text(uint8_t reason) {
    switch (reason) {
        case 2: return "router nie odpowiedział";
        case 8: return "rozłączono";
        case 15: case 204: return "złe hasło?";
        case 200: return "utrata sygnału";
        case 201: return "nie znaleziono sieci";
        case 202: return "odmowa uwierzytelnienia";
        case 205: return "nieudane łączenie";
        default: return "inny";
    }
}

void on_back(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::show_page(context.wifi.page, false);
    context.wifi.open = false;
}

void on_rescan(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    context.network->scan();
    render(context);
}

void forget_network(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    context.network->forget();
    render(context);
}

// Zapomnienie sieci odcina pilota od domu, a odzyskanie wymaga wpisania hasła,
// więc jedno dotknięcie wiersza tylko pyta.
void on_forget(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    // Etykieta kopiuje tekst, więc bufor może być lokalny. Bez cudzysłowów
    // drukarskich: krój ich nie zawiera.
    char message[160];
    std::snprintf(message, sizeof(message),
                  "Pilot rozłączy się z siecią %s i nie połączy się z nią sam. "
                  "Żeby wrócić, trzeba będzie wpisać hasło.",
                  context.network->remembered_ssid().c_str());
    widgets::ask_confirm("Zapomnieć sieć?", message, "Zapomnij", &forget_network, &context);
}

// Hasło wpisuje się wspólnym edytorem, tym samym co pola brokera. Edytor leży
// na warstwie systemowej, nad podstronami; własna klawiatura tego ekranu leżała
// pod nimi i po wybraniu sieci była niewidoczna.
void on_password_ready(void* raw, const char* value) {
    auto& context = *static_cast<Context*>(raw);
    transport::Password password;
    password.assign(value);
    context.network->connect(context.wifi.selected_ssid, password);
    render(context);
}

struct NetworkBinding {
    Context* context;
    std::size_t index;
};

void on_network_row(lv_event_t* event) {
    auto* binding = static_cast<NetworkBinding*>(lv_event_get_user_data(event));
    Context& context = *binding->context;
    if (binding->index >= context.network->access_point_count()) {
        return;
    }
    const transport::AccessPoint& ap = context.network->access_points()[binding->index];
    context.wifi.selected_ssid = ap.ssid;
    if (ap.secured) {
        widgets::open_text_editor(*context.editor, "HASŁO SIECI", ap.ssid.c_str(), "",
                                  widgets::EditorMode::Secret, &on_password_ready, &context);
    } else {
        context.network->connect(ap.ssid, transport::Password{});
        render(context);
    }
}

}  // namespace

void build_wifi(Context& context) {
    WifiScreen& screen = context.wifi;

    screen.page = widgets::make_page("Wi-Fi", &on_back, &context);

    screen.status = widgets::make_readout(screen.page.body, "Stan");
    screen.signal = widgets::make_readout(screen.page.body, "Sygnał");
    screen.link = widgets::make_readout(screen.page.body, "Od startu");

    screen.current = widgets::make_list_row(screen.page.body, LV_SYMBOL_WIFI, "—", "", "",
                                            &on_forget, &context);
    screen.rescan = widgets::make_action(screen.page.body, LV_SYMBOL_REFRESH, "Szukaj sieci",
                                         false, widgets::ActionLayout::Block, &on_rescan,
                                         &context);

    screen.hint = widgets::make_cap(screen.page.body, "DOSTĘPNE SIECI");

    screen.list = lv_obj_create(screen.page.body);
    theme::make_flat(screen.list);
    lv_obj_set_width(screen.list, LV_PCT(100));
    lv_obj_set_flex_grow(screen.list, 1);
    lv_obj_add_flag(screen.list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(screen.list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(screen.list, LV_SCROLLBAR_MODE_OFF);
    theme::make_column(screen.list, theme::kS2);

    // Wiersze powstają raz; skan zmienia ich treść, a nie ich liczbę.
    for (std::size_t i = 0; i < kMaxNetworkRows; ++i) {
        screen.rows[i] = widgets::make_list_row(screen.list, LV_SYMBOL_WIFI, "", "", "", nullptr,
                                                nullptr);
        lv_obj_add_flag(screen.rows[i].root, LV_OBJ_FLAG_HIDDEN);

        auto* binding = static_cast<NetworkBinding*>(lv_malloc(sizeof(NetworkBinding)));
        binding->context = &context;
        binding->index = i;
        lv_obj_add_event_cb(screen.rows[i].root, on_network_row, LV_EVENT_CLICKED, binding);
        lv_obj_add_event_cb(
            screen.rows[i].root,
            [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); }, LV_EVENT_DELETE,
            binding);
    }

}

void open_wifi_page(Context& context) {
    context.wifi.open = true;
    widgets::show_page(context.wifi.page, true);
    // Skan przez koprocesor potrafi zerwać łącze, więc przy działającym
    // połączeniu nie uruchamiamy go sami. Szukanie sieci zostaje decyzją
    // użytkownika — to on wie, czy chce zmieniać sieć.
    if (context.network->phase() != transport::WifiPhase::Connected) {
        context.network->scan();
    }
    render(context);
}

void render_wifi(Context& context) {
    WifiScreen& screen = context.wifi;
    const auto phase = context.network->phase();

    screen.status.set(phase_text(phase, context.network->error()),
                      phase == transport::WifiPhase::Connected);

    const bool connected = phase == transport::WifiPhase::Connected;

    char text[96];
    if (connected) {
        std::snprintf(text, sizeof(text), "%d dBm · %s",
                      static_cast<int>(context.network->current_rssi()),
                      signal_text(context.network->current_rssi()));
        screen.signal.set(text, context.network->current_rssi() >= -78);
    } else {
        screen.signal.set("—", false);
    }
    const uint8_t reason = context.network->last_reason();
    if (reason != 0) {
        std::snprintf(text, sizeof(text), "zerwania %u · nieudane %u · powód %u (%s)",
                      static_cast<unsigned>(context.network->drops()),
                      static_cast<unsigned>(context.network->failures()),
                      static_cast<unsigned>(reason), reason_text(reason));
    } else {
        std::snprintf(text, sizeof(text), "zerwania %u · nieudane %u",
                      static_cast<unsigned>(context.network->drops()),
                      static_cast<unsigned>(context.network->failures()));
    }
    screen.link.set(text, context.network->drops() == 0 && context.network->failures() == 0);

    const transport::Ssid& remembered = context.network->remembered_ssid();
    if (connected) {
        lv_label_set_text(screen.current.title, context.network->current_ssid().c_str());
        lv_label_set_text(screen.current.note, "dotknij, aby zapomnieć tę sieć");
        lv_label_set_text(screen.current.value, signal_text(context.network->current_rssi()));
        lv_obj_remove_flag(screen.current.root, LV_OBJ_FLAG_HIDDEN);
    } else if (!remembered.empty()) {
        lv_label_set_text(screen.current.title, remembered.c_str());
        lv_label_set_text(screen.current.note, "zapamiętana · dotknij, aby zapomnieć");
        lv_label_set_text(screen.current.value, "");
        lv_obj_remove_flag(screen.current.root, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(screen.current.root, LV_OBJ_FLAG_HIDDEN);
    }

    // Gdy sieć stoi, mówimy wprost, dlaczego lista jest pusta.
    const std::size_t count = context.network->access_point_count();
    if (count == 0 && connected) {
        lv_label_set_text(screen.hint, "SZUKANIE SIECI PRZERYWA POŁĄCZENIE");
    } else if (context.network->scanning()) {
        lv_label_set_text(screen.hint, "SZUKAM…");
    } else {
        lv_label_set_text(screen.hint, "DOSTĘPNE SIECI");
    }

    for (std::size_t i = 0; i < kMaxNetworkRows; ++i) {
        if (i >= count) {
            lv_obj_add_flag(screen.rows[i].root, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const transport::AccessPoint& ap = context.network->access_points()[i];
        lv_obj_remove_flag(screen.rows[i].root, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(screen.rows[i].title, ap.ssid.c_str());
        lv_label_set_text(screen.rows[i].note, ap.secured ? "zabezpieczona" : "otwarta");
        lv_label_set_text(screen.rows[i].value, signal_text(ap.rssi));
    }
}

}  // namespace ui
