#include <algorithm>

#include "images.h"
#include "ui_internal.hpp"

namespace ui {
namespace {

// Skróty kanałowe idą po nazwie kontraktem v1 do Home Assistanta, który
// przełącza kanał na Glasshouse (ADR 0008). Nie mają stanu do potwierdzenia.
// Ostatni kafelek otwiera menu wszystkich kanałów.
const widgets::TileSpec kChannels[5] = {
    {&logo_polsat, nullptr, nullptr},
    {&logo_tvn, nullptr, nullptr},
    {&logo_tvp1, nullptr, nullptr},
    {&logo_tvp2, nullptr, nullptr},
    {nullptr, LV_SYMBOL_LIST, nullptr},
};
constexpr std::size_t kChannelMenuTile = 4;

// Nazwy kanałów tak, jak podaje je tuner. Porównanie pomija wielkość liter i spacje.
const char* const kChannelIds[4] = {"Polsat", "TVN", "TVP1", "TVP2"};

// Krok głośności Z906 jak na tarczy ekranu dźwięku.
constexpr int32_t kVolumeStep = 2;

// Identyfikator opcji o danym indeksie albo pusty, gdy źródło ich nie podaje.
const char* id_at(const domain::OptionList& ids, std::size_t index) {
    return index < ids.size() ? ids[index].c_str() : "";
}

// Lista opcji, identyfikatory i bieżąca wartość sterowania, które obsługuje lista wyboru.
struct PickerData {
    const domain::OptionList* options;
    const domain::OptionList* ids;
    const domain::Label* current;
};

PickerData picker_data(const domain::TvState& tv, domain::ControlId control) {
    if (control == domain::ControlId::TvSource) {
        return {&tv.source_options, &tv.source_ids, &tv.source};
    }
    return {&tv.picture_mode_options, &tv.picture_mode_ids, &tv.picture_mode};
}

void open_picker(Context& context, domain::ControlId control, const char* title) {
    const PickerData data = picker_data(context.controller->state().snapshot().tv, control);
    if (data.options->empty()) {
        return;
    }
    context.tv.picker_control = control;
    lv_label_set_text(context.tv.picker.title, title);
    widgets::show_page(context.tv.picker, true);
    render(context);
}

void on_source(void* raw, std::size_t) {
    open_picker(*static_cast<Context*>(raw), domain::ControlId::TvSource, "Źródło");
}

void on_picture(void* raw, std::size_t) {
    open_picker(*static_cast<Context*>(raw), domain::ControlId::TvPicture, "Tryb obrazu");
}

void on_picker_back(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    widgets::show_page(context.tv.picker, false);
}

void on_picker_row(void* raw) {
    auto& row = *static_cast<TvScreen::PickerRow*>(raw);
    Context& context = *row.context;
    const domain::ControlId control = context.tv.picker_control;
    const PickerData data = picker_data(context.controller->state().snapshot().tv, control);
    if (row.index >= data.options->size()) {
        return;
    }
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetOption;
    intent.control = control;
    // Nazwa służy oczekiwaniu na ekranie, identyfikator — poleceniu.
    intent.text = (*data.options)[row.index];
    intent.payload.assign(id_at(*data.ids, row.index));
    // Lista znika od razu; oczekiwanie widać na segmencie w rzędzie.
    widgets::show_page(context.tv.picker, false);
    dispatch(context, intent);
}

void on_channel_row(void* raw);

// Wiersz kanału: numer i nazwa na przycisku w stylu wiersza listy. Wygląda jak
// ListRow bez ikony, opisu i strzałki, ale ma trzy obiekty zamiast siedmiu.
void make_channel_row(Context& context, std::size_t index) {
    TvScreen::ChannelRow& row = context.tv.channel_rows[index];
    row.context = &context;
    row.index = index;

    row.root = lv_button_create(context.tv.channel_page.body);
    lv_obj_remove_style_all(row.root);
    lv_obj_add_style(row.root, &theme::styles().list_row, LV_PART_MAIN);
    lv_obj_add_style(row.root, &theme::styles().pressed_soft, LV_STATE_PRESSED);
    lv_obj_set_size(row.root, LV_PCT(100), 76);
    theme::make_row(row.root, theme::kS3);

    row.number = lv_label_create(row.root);
    lv_obj_remove_style_all(row.number);
    lv_obj_set_width(row.number, 44);
    lv_obj_set_style_text_font(row.number, theme::font_b15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(row.number, theme::dim(), LV_PART_MAIN);
    lv_obj_set_style_text_align(row.number, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    row.name = lv_label_create(row.root);
    lv_obj_remove_style_all(row.name);
    lv_obj_set_flex_grow(row.name, 1);
    lv_label_set_long_mode(row.name, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_font(row.name, theme::font_m15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(row.name, theme::text(), LV_PART_MAIN);

    lv_obj_add_event_cb(
        row.root, [](lv_event_t* event) { on_channel_row(lv_event_get_user_data(event)); },
        LV_EVENT_CLICKED, &row);
}

// Przepisuje wiersze, gdy przyszła nowa lista. Brakujące wiersze dobudowuje.
void sync_channel_rows(Context& context) {
    TvScreen& screen = context.tv;
    const domain::ChannelList* list = context.controller->channels();
    const std::size_t count = list != nullptr ? list->count : 0;
    const uint32_t revision = list != nullptr ? list->revision : 0;
    if (screen.channel_rows_built >= count && revision == screen.channel_revision_shown) {
        return;
    }
    while (screen.channel_rows_built < count) {
        make_channel_row(context, screen.channel_rows_built++);
    }
    for (std::size_t i = 0; i < screen.channel_rows_built; ++i) {
        const TvScreen::ChannelRow& row = screen.channel_rows[i];
        if (i >= count) {
            lv_obj_add_flag(row.root, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const domain::Channel& channel = list->items[i];
        lv_obj_remove_flag(row.root, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(row.number, channel.number.c_str());
        lv_label_set_text(row.name, channel.name.c_str());
    }
    if (count == 0) {
        lv_obj_remove_flag(screen.channel_empty, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(screen.channel_empty, LV_OBJ_FLAG_HIDDEN);
    }
    screen.channel_revision_shown = revision;
}

void on_channel_back(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    context.tv.channel_page_open = false;
    widgets::show_page(context.tv.channel_page, false);
}

void on_channel_row(void* raw) {
    auto& row = *static_cast<TvScreen::ChannelRow*>(raw);
    Context& context = *row.context;
    const domain::ChannelList* list = context.controller->channels();
    if (list == nullptr || row.index >= list->count) {
        return;
    }
    const domain::Channel& channel = list->items[row.index];
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::Momentary;
    intent.control = domain::ControlId::TvChannel;
    intent.text = channel.name;
    intent.payload.assign(channel.id.c_str());
    on_channel_back(&context);
    dispatch(context, intent);
}

void on_channel(void* raw, std::size_t index) {
    auto& context = *static_cast<Context*>(raw);
    if (index == kChannelMenuTile) {
        context.tv.channel_page_open = true;
        sync_channel_rows(context);
        widgets::show_page(context.tv.channel_page, true);
        return;
    }
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::Momentary;
    intent.control = domain::ControlId::TvChannel;
    intent.text.assign(kChannelIds[index]);
    dispatch(context, intent);
}

void on_key(void* raw, domain::NavKey key) {
    auto& context = *static_cast<Context*>(raw);
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::Navigate;
    intent.key = key;
    intent.target.assign("tv");
    dispatch(context, intent);
}

// VOL steruje głośnością Z906, bo dźwięk TV idzie optykiem do wzmacniacza.
// Kolejny krok liczymy od wartości w toku, żeby szybkie stuknięcia się sumowały.
void step_volume(Context& context, int32_t delta) {
    const domain::AppState& state = context.controller->state();
    if (!state.z906_fresh()) {
        return;
    }
    int32_t value = state.snapshot().z906.volume_percent;
    if (const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ZVolume)) {
        value = pending->requested_value;
    }
    const int32_t limit = volume_limit();
    value = std::max<int32_t>(0, std::min<int32_t>(value + delta, limit));
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetValue;
    intent.control = domain::ControlId::ZVolume;
    intent.value = static_cast<uint8_t>(value);
    dispatch(context, intent);
}

void on_volume_up(void* raw) { step_volume(*static_cast<Context*>(raw), kVolumeStep); }
void on_volume_down(void* raw) { step_volume(*static_cast<Context*>(raw), -kVolumeStep); }

void step_channel(Context& context, bool up) {
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::Momentary;
    intent.control = domain::ControlId::TvChannelStep;
    intent.flag = up;
    dispatch(context, intent);
}

void on_channel_up(void* raw) { step_channel(*static_cast<Context*>(raw), true); }
void on_channel_down(void* raw) { step_channel(*static_cast<Context*>(raw), false); }

void on_back(void* raw) { on_key(raw, domain::NavKey::Back); }
void on_menu(void* raw) { on_key(raw, domain::NavKey::Menu); }

// Segment pokazuje wybór w toku, potem bieżącą wartość z TV. Zaznaczenie i obwódka
// pochodzą ze stanu, nie z dotknięcia.
void render_half(widgets::Segmented& segmented, const domain::AppState& state,
                 domain::ControlId control, const domain::Label& current, const char* fallback) {
    const domain::PendingEntry* pending = state.pending_of(control);
    const char* label = fallback;
    if (pending != nullptr && !pending->requested.empty()) {
        label = pending->requested.c_str();
    } else if (state.tv_fresh() && !current.empty()) {
        label = current.c_str();
    }
    segmented.set_labels(&label, 1);
    if (state.tv_fresh() && !current.empty()) {
        segmented.set_selected(0);
    } else {
        segmented.set_selected_none();
    }
    segmented.set_status(0, state.status_of(control));
}

}  // namespace

void build_tv(Context& context, lv_obj_t* parent) {
    TvScreen& screen = context.tv;
    screen.section = widgets::make_section(parent);

    screen.banner = widgets::make_banner(screen.section,
                                         "Brak świeżego stanu telewizora.\n"
                                         "Sterowanie wróci po nowym snapshocie.");

    lv_obj_t* selectors = widgets::make_duo(screen.section);
    lv_obj_set_height(selectors, LV_SIZE_CONTENT);
    screen.source = widgets::make_value_segment(selectors, LV_SYMBOL_CHARGE, &on_source, &context);
    screen.picture = widgets::make_value_segment(selectors, LV_SYMBOL_IMAGE, &on_picture, &context);

    // Kwadrat 80 × 80: logo ma najwyżej 38 px, wyższy kafelek wyglądał na wyciągnięty.
    screen.channels = widgets::make_tiles(screen.section, kChannels, 5, 5, 80, &on_channel,
                                          &context);

    // Kołyski po bokach krzyżaka, jak na pilocie LG: VOL po lewej, P po prawej.
    lv_obj_t* fill = widgets::make_fill(screen.section);
    lv_obj_set_style_pad_column(fill, theme::kS4, LV_PART_MAIN);
    widgets::make_rocker(fill, "VOL", true, &on_volume_up, &on_volume_down, &context);
    widgets::make_dpad(fill, &on_key, &context);
    widgets::make_rocker(fill, "P", false, &on_channel_up, &on_channel_down, &context);

    lv_obj_t* duo = widgets::make_duo(screen.section);
    widgets::make_action(duo, LV_SYMBOL_LEFT, "Cofnij", false, widgets::ActionLayout::Fill, &on_back, &context);
    widgets::make_action(duo, LV_SYMBOL_LIST, "Menu TV", false, widgets::ActionLayout::Fill, &on_menu, &context);

    screen.channel_page = widgets::make_page("Kanały", &on_channel_back, &context);
    screen.channel_empty = widgets::make_cap(screen.channel_page.body,
                                             "LISTA KANAŁÓW JESZCZE NIE DOTARŁA");

    screen.picker = widgets::make_page("", &on_picker_back, &context);
    for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
        TvScreen::PickerRow& row = screen.picker_rows[i];
        row.context = &context;
        row.index = i;
        row.row = widgets::make_list_row(screen.picker.body, "", "", "", "", &on_picker_row,
                                         &row);
        // Wiersz wyboru, nie przejście dalej: bez ikony i strzałki. Pusty opis
        // spychał nazwę w górę, więc chowamy go, żeby nazwa stała na środku.
        lv_obj_add_flag(lv_obj_get_child(row.row.root, 0), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lv_obj_get_child(row.row.root, -1), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(row.row.note, LV_OBJ_FLAG_HIDDEN);
    }
}

void render_tv(Context& context) {
    TvScreen& screen = context.tv;
    const domain::AppState& state = context.controller->state();
    const domain::TvState& tv = state.snapshot().tv;

    render_half(screen.source, state, domain::ControlId::TvSource, tv.source, "Źródło");
    render_half(screen.picture, state, domain::ControlId::TvPicture, tv.picture_mode,
                "Tryb obrazu");

    // Lista wyboru: opcje z bieżącego stanu, dla trybu obrazu tylko te, które
    // telewizor przyjmie dla bieżącego materiału.
    if (screen.picker_control != domain::ControlId::None) {
        const PickerData data = picker_data(tv, screen.picker_control);
        for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
            const widgets::ListRow& row = screen.picker_rows[i].row;
            if (i >= data.options->size()) {
                lv_obj_add_flag(row.root, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(row.root, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(row.title, (*data.options)[i].c_str());
            const bool current = state.tv_fresh() && (*data.options)[i] == *data.current;
            lv_label_set_text(row.value, current ? "aktywne" : "");
            lv_obj_set_style_border_color(row.root, current ? theme::accent() : theme::line(),
                                          LV_PART_MAIN);
        }
    }

    if (screen.channel_page_open) {
        sync_channel_rows(context);
    }

    widgets::set_unavailable(screen.section, screen.banner, !state.tv_fresh());
}

}  // namespace ui
