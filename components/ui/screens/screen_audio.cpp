#include <algorithm>

#include "domain/z906_options.hpp"
#include "ui_internal.hpp"

namespace ui {
namespace {

// Efekt po polsku tam, gdzie kontroler mówi po angielsku. Polecenie idzie zawsze
// pełną nazwą z listy (domain/z906_options.hpp).
const char* effect_name(const domain::Label& full) {
    return full == "No effect" ? "Bez efektu" : full.c_str();
}

void send_option(Context& context, domain::ControlId control, const domain::Label& value) {
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetOption;
    intent.control = control;
    intent.text = value;
    dispatch(context, intent);
}

const domain::OptionList& picker_options(domain::ControlId control) {
    return control == domain::ControlId::ZSource ? domain::z906_sources() : domain::z906_effects();
}

void open_picker(Context& context, domain::ControlId control, const char* title) {
    context.audio.picker_control = control;
    lv_label_set_text(context.audio.picker.title, title);
    widgets::show_page(context.audio.picker, true);
    render(context);
}

void on_source(void* raw, std::size_t) {
    open_picker(*static_cast<Context*>(raw), domain::ControlId::ZSource, "Źródło dźwięku");
}

void on_effect(void* raw, std::size_t) {
    open_picker(*static_cast<Context*>(raw), domain::ControlId::ZEffect, "Efekt");
}

void on_picker_back(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    context.audio.picker_control = domain::ControlId::None;
    widgets::show_page(context.audio.picker, false);
}

void on_picker_row(void* raw) {
    auto& row = *static_cast<AudioScreen::PickerRow*>(raw);
    Context& context = *row.context;
    const domain::ControlId control = context.audio.picker_control;
    const domain::OptionList& options = picker_options(control);
    if (row.index >= options.size()) {
        return;
    }
    // Lista znika od razu; oczekiwanie widać na segmencie w rzędzie.
    on_picker_back(&context);
    send_option(context, control, options[row.index]);
}

// Segment pokazuje wybór w toku, potem bieżącą wartość z kontrolera.
void render_value(widgets::Segmented& segmented, const domain::AppState& state,
                  domain::ControlId control, const domain::Label& current, const char* fallback,
                  bool effect) {
    const domain::PendingEntry* pending = state.pending_of(control);
    const bool fresh = state.z906_fresh() && !current.empty();
    const char* label = fallback;
    if (pending != nullptr && !pending->requested.empty()) {
        label = effect ? effect_name(pending->requested) : pending->requested.c_str();
    } else if (fresh) {
        label = effect ? effect_name(current) : current.c_str();
    }
    segmented.set_labels(&label, 1);
    if (fresh) {
        segmented.set_selected(0);
    } else {
        segmented.set_selected_none();
    }
    segmented.set_status(0, state.status_of(control));
}

void send_value(Context& context, domain::ControlId control, int32_t value) {
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetValue;
    intent.control = control;
    intent.value = static_cast<uint8_t>(value);
    dispatch(context, intent);
}

void on_volume(void* raw, int32_t value, bool) {
    // Limit z ustawień „Dźwięk” (domyślnie 100 %, czyli bez limitu).
    const int32_t limit = volume_limit();
    send_value(*static_cast<Context*>(raw), domain::ControlId::ZVolume,
               value > limit ? limit : value);
}
// −/+ obok tarczy: krok o jeden punkt. Kolejny liczymy od wartości w toku, żeby
// szybkie stuknięcia i przytrzymanie się sumowały, zanim snapshot je potwierdzi.
// Kontroler przyjmuje dowolny procent, choć encja w HA deklaruje krok 2.
constexpr int32_t kVolumeStep = 1;

void step_volume(Context& context, int32_t delta) {
    const domain::AppState& state = context.controller->state();
    if (!state.z906_fresh()) {
        return;
    }
    int32_t value = state.snapshot().z906.volume_percent;
    if (const domain::PendingEntry* pending = state.pending_of(domain::ControlId::ZVolume)) {
        value = pending->requested_value;
    }
    const int32_t next = std::clamp<int32_t>(value + delta, 0, volume_limit());
    if (next == value) {
        return;
    }
    // Tarcza pokazuje nową wartość od razu; snapshot ją potem potwierdzi.
    context.audio.dial->set_value(next);
    send_value(context, domain::ControlId::ZVolume, next);
}

void on_volume_down(void* raw) { step_volume(*static_cast<Context*>(raw), -kVolumeStep); }
void on_volume_up(void* raw) { step_volume(*static_cast<Context*>(raw), kVolumeStep); }

void on_center(void* raw, int32_t value, bool) {
    send_value(*static_cast<Context*>(raw), domain::ControlId::ZCenter, value);
}
void on_subwoofer(void* raw, int32_t value, bool) {
    send_value(*static_cast<Context*>(raw), domain::ControlId::ZSubwoofer, value);
}
void on_rear(void* raw, int32_t value, bool) {
    send_value(*static_cast<Context*>(raw), domain::ControlId::ZRear, value);
}

void on_mute(void* raw) {
    auto& context = *static_cast<Context*>(raw);
    domain::UiIntent intent;
    intent.kind = domain::IntentKind::SetMuted;
    intent.control = domain::ControlId::ZMute;
    intent.flag = !context.controller->state().snapshot().z906.muted;
    dispatch(context, intent);
}

// Suwak pokazuje wartość ze snapshotu, chyba że palec właśnie ją przesuwa —
// wtedy prowadzi gest, a snapshot domyka sprawę po potwierdzeniu.
void apply_level(widgets::Level* level, int32_t snapshot_value, domain::ControlStatus status) {
    if (status == domain::ControlStatus::Idle) {
        level->set_value(snapshot_value);
    }
    level->set_status(status);
}

}  // namespace

void build_audio(Context& context, lv_obj_t* parent) {
    AudioScreen& screen = context.audio;
    screen.section = widgets::make_section(parent);

    screen.banner = widgets::make_banner(screen.section,
                                         "Brak świeżego stanu Z906.\n"
                                         "Sterowanie wróci po nowym snapshocie.");

    lv_obj_t* selectors = widgets::make_duo(screen.section);
    lv_obj_set_height(selectors, LV_SIZE_CONTENT);
    screen.source = widgets::make_value_segment(selectors, LV_SYMBOL_USB, &on_source, &context);
    screen.effect = widgets::make_value_segment(selectors, LV_SYMBOL_AUDIO, &on_effect, &context);

    // −/+ w wolnym miejscu po bokach tarczy, z powtarzaniem przy przytrzymaniu.
    lv_obj_t* fill = widgets::make_fill(screen.section);
    lv_obj_set_style_pad_column(fill, theme::kS5, LV_PART_MAIN);
    widgets::make_step_button(fill, LV_SYMBOL_MINUS, true, &on_volume_down, &context);
    screen.dial = widgets::make_dial(fill, "głośność", &on_volume, &context);
    widgets::make_step_button(fill, LV_SYMBOL_PLUS, true, &on_volume_up, &context);

    // Wyciszenie siedzi na dole tarczy, tak jak w makiecie.
    screen.mute = lv_button_create(screen.dial->root);
    lv_obj_remove_style_all(screen.mute);
    lv_obj_set_size(screen.mute, 64, 64);
    lv_obj_align(screen.mute, LV_ALIGN_BOTTOM_MID, 0, -2);
    lv_obj_set_style_radius(screen.mute, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.mute, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.mute, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_border_width(screen.mute, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(screen.mute, theme::line(), LV_PART_MAIN);
    lv_obj_set_flex_flow(screen.mute, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(screen.mute, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_t* mute_icon = lv_label_create(screen.mute);
    lv_obj_remove_style_all(mute_icon);
    lv_label_set_text(mute_icon, LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_font(mute_icon, theme::font_icon(), LV_PART_MAIN);
    lv_obj_set_style_text_color(mute_icon, theme::dim(), LV_PART_MAIN);
    lv_obj_add_event_cb(
        screen.mute, [](lv_event_t* event) { on_mute(lv_event_get_user_data(event)); },
        LV_EVENT_CLICKED, &context);

    lv_obj_t* levels = lv_obj_create(screen.section);
    theme::make_flat(levels);
    lv_obj_set_width(levels, LV_PCT(100));
    lv_obj_set_height(levels, LV_SIZE_CONTENT);
    theme::make_row(levels, theme::kS2);
    lv_obj_set_flex_align(levels, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);

    screen.center = widgets::make_level(levels, "Center", &on_center, &context);
    screen.subwoofer = widgets::make_level(levels, "Subwoofer", &on_subwoofer, &context);
    screen.rear = widgets::make_level(levels, "Tył", &on_rear, &context);

    screen.picker = widgets::make_page("", &on_picker_back, &context);
    for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
        AudioScreen::PickerRow& row = screen.picker_rows[i];
        row.context = &context;
        row.index = i;
        row.row = widgets::make_list_row(screen.picker.body, "", "", "", "", &on_picker_row,
                                         &row);
        // Wiersz wyboru, nie przejście dalej: bez ikony, strzałki i opisu.
        lv_obj_add_flag(lv_obj_get_child(row.row.root, 0), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(lv_obj_get_child(row.row.root, -1), LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(row.row.note, LV_OBJ_FLAG_HIDDEN);
    }
}

void render_audio(Context& context) {
    AudioScreen& screen = context.audio;
    const domain::AppState& state = context.controller->state();
    const domain::Z906State& z906 = state.snapshot().z906;

    render_value(screen.source, state, domain::ControlId::ZSource, z906.source, "Źródło", false);
    render_value(screen.effect, state, domain::ControlId::ZEffect, z906.effect, "Efekt", true);

    // Lista wyboru: zaznaczone jest to, co potwierdził kontroler.
    if (screen.picker_control != domain::ControlId::None) {
        const bool effect = screen.picker_control == domain::ControlId::ZEffect;
        const domain::OptionList& options = picker_options(screen.picker_control);
        const domain::Label& current = effect ? z906.effect : z906.source;
        for (std::size_t i = 0; i < domain::kMaxOptions; ++i) {
            const widgets::ListRow& row = screen.picker_rows[i].row;
            if (i >= options.size()) {
                lv_obj_add_flag(row.root, LV_OBJ_FLAG_HIDDEN);
                continue;
            }
            lv_obj_remove_flag(row.root, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(row.title, effect ? effect_name(options[i]) : options[i].c_str());
            const bool active = state.z906_fresh() && options[i] == current;
            lv_label_set_text(row.value, active ? "aktywne" : "");
            lv_obj_set_style_border_color(row.root, active ? theme::accent() : theme::line(),
                                          LV_PART_MAIN);
        }
    }

    const domain::ControlStatus volume_status = state.status_of(domain::ControlId::ZVolume);
    if (volume_status == domain::ControlStatus::Idle) {
        screen.dial->set_value(z906.volume_percent);
    }
    screen.dial->set_status(volume_status);

    apply_level(screen.center, z906.center_percent, state.status_of(domain::ControlId::ZCenter));
    apply_level(screen.subwoofer, z906.subwoofer_percent,
                state.status_of(domain::ControlId::ZSubwoofer));
    apply_level(screen.rear, z906.rear_percent, state.status_of(domain::ControlId::ZRear));

    lv_obj_t* mute_icon = lv_obj_get_child(screen.mute, 0);
    lv_label_set_text(mute_icon, z906.muted ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
    lv_obj_set_style_text_color(mute_icon, z906.muted ? theme::danger() : theme::dim(),
                                LV_PART_MAIN);
    lv_obj_set_style_bg_color(screen.mute, z906.muted ? theme::danger() : theme::surface2(),
                              LV_PART_MAIN);
    lv_obj_set_style_bg_opa(screen.mute, z906.muted ? theme::kDangerSoftOpa : LV_OPA_COVER,
                            LV_PART_MAIN);
    widgets::set_status_outline(screen.mute, state.status_of(domain::ControlId::ZMute));


    widgets::set_unavailable(screen.section, screen.banner, !state.z906_fresh());
}

}  // namespace ui
