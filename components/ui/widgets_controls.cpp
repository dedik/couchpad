#include "widgets_internal.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

namespace ui::widgets {

using namespace detail;

namespace {

// Tarcza wypełnia miejsce po Decode 5.1 i rzędzie efektów (~320 px wysokości).
// −/+ siedzą w otwarciu łuku u dołu, więc boki są wolne (2026-11-03).
constexpr int32_t kDialSize = 296;
constexpr int32_t kDialRing = 22;
constexpr int32_t kDialKnobPad = 3;     // uchwyt = szerokość łuku + 2 × 3 = 28 px
constexpr int32_t kDialDeadZone = 30;   // środek tarczy: kąt jest tam niestabilny
constexpr int32_t kLevelTrackW = 56;
constexpr int32_t kLevelTrackH = 170;
constexpr int32_t kLevelTravel = 180;   // 180 px ruchu w pionie to pełny zakres

}  // namespace

// ============================= tarcza =============================

void Dial::set_value(int32_t value) {
    value_ = clamp100(value);
    lv_arc_set_value(arc, value_);
    lv_label_set_text_fmt(value_label, "%d", static_cast<int>(value_));
}

void Dial::set_status(domain::ControlStatus status) {
    // Oczekiwanie bez wyróżnienia: zmianę widać na łuku. Kolor tylko przy błędzie.
    lv_obj_set_style_text_color(
        value_label,
        status == domain::ControlStatus::Failed ? theme::danger() : theme::text(),
        LV_PART_MAIN);
}

namespace {

float angle_at(lv_obj_t* root, int32_t* out_distance) {
    lv_indev_t* indev = lv_indev_active();
    lv_point_t point{};
    lv_indev_get_point(indev, &point);

    lv_area_t area;
    lv_obj_get_coords(root, &area);
    const float cx = static_cast<float>(area.x1 + area.x2) / 2.0F;
    const float cy = static_cast<float>(area.y1 + area.y2) / 2.0F;
    const float dx = static_cast<float>(point.x) - cx;
    const float dy = static_cast<float>(point.y) - cy;
    if (out_distance != nullptr) {
        *out_distance = static_cast<int32_t>(std::sqrt(dx * dx + dy * dy));
    }
    return std::atan2(dy, dx) * 180.0F / 3.14159265F;
}

void dial_event(lv_event_t* event) {
    auto* dial = static_cast<Dial*>(lv_event_get_user_data(event));
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        dial->last_angle_ = angle_at(dial->root, nullptr);
        dial->accumulator_ = static_cast<float>(dial->value_);
        dial->moved_ = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        int32_t distance = 0;
        const float angle = angle_at(dial->root, &distance);
        if (distance < kDialDeadZone) {
            dial->last_angle_ = angle;
            return;
        }
        float delta = angle - dial->last_angle_;
        if (delta > 180.0F) {
            delta -= 360.0F;
        }
        if (delta < -180.0F) {
            delta += 360.0F;
        }
        dial->last_angle_ = angle;

        // Pełny obrót wokół środka to 100 punktów.
        dial->accumulator_ = std::clamp(dial->accumulator_ + delta / 360.0F * 100.0F, 0.0F, 100.0F);
        const int32_t next = step2(static_cast<int32_t>(dial->accumulator_));
        if (next != dial->value_) {
            dial->moved_ = true;
            dial->set_value(next);
            dial->set_status(domain::ControlStatus::Pending);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (dial->moved_ && dial->on_change != nullptr) {
            // Jedno polecenie po puszczeniu palca — wartość końcowa, nie strumień.
            dial->on_change(dial->context, dial->value_, true);
        }
        dial->moved_ = false;
    }
}

}  // namespace

Dial* make_dial(lv_obj_t* parent, const char* caption, ValueHandler on_change, void* context) {
    auto* dial = static_cast<Dial*>(lv_malloc_zeroed(sizeof(Dial)));
    dial->on_change = on_change;
    dial->context = context;

    // Powierzchnia dotyku leży nad łukiem, bo lv_arc sam w sobie skacze do palca.
    dial->root = plain(parent);
    lv_obj_set_size(dial->root, kDialSize, kDialSize);
    lv_obj_add_flag(dial->root, LV_OBJ_FLAG_CLICKABLE);

    dial->arc = lv_arc_create(dial->root);
    lv_obj_remove_style_all(dial->arc);
    // Łuk mniejszy o wystający uchwyt, żeby rodzic go nie przycinał.
    lv_obj_set_size(dial->arc, kDialSize - 2 * kDialKnobPad, kDialSize - 2 * kDialKnobPad);
    lv_obj_center(dial->arc);
    lv_obj_remove_flag(dial->arc, LV_OBJ_FLAG_CLICKABLE);
    // Łuk z makiety zaczyna się na 225° od godziny dwunastej i obejmuje 270°.
    lv_arc_set_bg_angles(dial->arc, 135, 45);
    lv_arc_set_range(dial->arc, 0, 100);
    lv_arc_set_value(dial->arc, 0);
    lv_obj_set_style_arc_width(dial->arc, kDialRing, LV_PART_MAIN);
    lv_obj_set_style_arc_color(dial->arc, theme::surface3(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(dial->arc, kDialRing, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(dial->arc, theme::accent(), LV_PART_INDICATOR);
    // Uchwyt na końcu wypełnienia: jasne koło z obwódką w kolorze akcentu.
    lv_obj_set_style_bg_opa(dial->arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(dial->arc, theme::text(), LV_PART_KNOB);
    lv_obj_set_style_radius(dial->arc, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(dial->arc, kDialKnobPad, LV_PART_KNOB);
    lv_obj_set_style_border_width(dial->arc, 4, LV_PART_KNOB);
    lv_obj_set_style_border_color(dial->arc, theme::accent(), LV_PART_KNOB);

    lv_obj_t* readout = plain(dial->root);
    dial->readout = readout;
    lv_obj_remove_flag(readout, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(readout, kDialSize - 2 * kDialRing - 16, LV_SIZE_CONTENT);
    lv_obj_center(readout);
    theme::make_column(readout, theme::kS2);
    lv_obj_set_flex_align(readout, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    dial->value_label = lv_label_create(readout);
    lv_obj_remove_style_all(dial->value_label);
    lv_obj_set_style_text_font(dial->value_label, theme::font_b64num(), LV_PART_MAIN);
    lv_obj_set_style_text_color(dial->value_label, theme::text(), LV_PART_MAIN);
    lv_label_set_text(dial->value_label, "0");

    dial->caption = make_cap(readout, caption);

    lv_obj_add_event_cb(dial->root, dial_event, LV_EVENT_PRESSED, dial);
    lv_obj_add_event_cb(dial->root, dial_event, LV_EVENT_PRESSING, dial);
    lv_obj_add_event_cb(dial->root, dial_event, LV_EVENT_RELEASED, dial);
    lv_obj_add_event_cb(dial->root, dial_event, LV_EVENT_PRESS_LOST, dial);
    lv_obj_add_event_cb(
        dial->root, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, dial);
    return dial;
}

// ============================= tarcza temperatury =============================

namespace {

constexpr int32_t kThermoSize = 300;
constexpr int32_t kThermoRing = 20;
constexpr int32_t kThermoKnobPad = 6;      // uchwyt = szerokość łuku + 2 × 6 = 32 px
constexpr int32_t kThermoDeadZone = 40;
// −/+ w otwarciu łuku u dołu, jak przy tarczy głośności (2026-11-03).
constexpr int32_t kThermoStepSize = 64;
constexpr int32_t kThermoStepGap = 24;
constexpr int32_t kThermoHeight = kThermoSize;
constexpr uint32_t kThermoCommitMs = 700;  // tyle po ostatnim stuknięciu idzie polecenie

void thermo_commit(lv_timer_t* timer) {
    auto* thermo = static_cast<Thermostat*>(lv_timer_get_user_data(timer));
    thermo->commit_timer_ = nullptr;
    if (thermo->on_change != nullptr) {
        thermo->on_change(thermo->context, thermo->value_, true);
    }
}

void thermo_stop_timer(Thermostat* thermo) {
    if (thermo->commit_timer_ != nullptr) {
        lv_timer_delete(thermo->commit_timer_);
        thermo->commit_timer_ = nullptr;
    }
}

// Zmiana lokalna od razu; wysyłka dopiero po chwili bez dotyku.
void thermo_nudge(Thermostat* thermo, int32_t direction) {
    if (!thermo->enabled_) {
        return;
    }
    const int32_t next = std::clamp(thermo->value_ + direction * thermo->step_, thermo->min_,
                                    thermo->max_);
    if (next == thermo->value_) {
        return;
    }
    thermo->set_value(next);
    thermo->set_status(domain::ControlStatus::Pending);
    thermo_stop_timer(thermo);
    thermo->commit_timer_ = lv_timer_create(thermo_commit, kThermoCommitMs, thermo);
    lv_timer_set_repeat_count(thermo->commit_timer_, 1);
}

void thermo_event(lv_event_t* event) {
    auto* thermo = static_cast<Thermostat*>(lv_event_get_user_data(event));
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        // Gest przejmuje serię stuknięć: wartość wyśle puszczenie palca.
        thermo_stop_timer(thermo);
        thermo->last_angle_ = angle_at(thermo->ring, nullptr);
        thermo->accumulator_ = static_cast<float>(thermo->value_);
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        int32_t distance = 0;
        const float angle = angle_at(thermo->ring, &distance);
        if (distance < kThermoDeadZone) {
            thermo->last_angle_ = angle;
            return;
        }
        float delta = angle - thermo->last_angle_;
        if (delta > 180.0F) {
            delta -= 360.0F;
        }
        if (delta < -180.0F) {
            delta += 360.0F;
        }
        thermo->last_angle_ = angle;

        // 270° łuku to cały zakres, tak jak go widać.
        const float span = static_cast<float>(thermo->max_ - thermo->min_);
        thermo->accumulator_ = std::clamp(thermo->accumulator_ + delta / 270.0F * span,
                                          static_cast<float>(thermo->min_),
                                          static_cast<float>(thermo->max_));
        const int32_t steps = static_cast<int32_t>(
            std::lround((thermo->accumulator_ - static_cast<float>(thermo->min_)) /
                        static_cast<float>(thermo->step_)));
        const int32_t next = std::clamp(thermo->min_ + steps * thermo->step_, thermo->min_,
                                        thermo->max_);
        if (next != thermo->value_) {
            thermo->moved_ = true;
            thermo->set_value(next);
            thermo->set_status(domain::ControlStatus::Pending);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (thermo->moved_ && thermo->on_change != nullptr) {
            thermo->on_change(thermo->context, thermo->value_, true);
        }
        thermo->moved_ = false;
    }
}

lv_obj_t* thermo_step_button(lv_obj_t* parent, const char* symbol, int32_t side) {
    lv_obj_t* button = lv_button_create(parent);
    lv_obj_remove_style_all(button);
    lv_obj_set_size(button, kThermoStepSize, kThermoStepSize);
    lv_obj_align(button, LV_ALIGN_BOTTOM_MID, side * (kThermoStepSize + kThermoStepGap) / 2, 0);
    lv_obj_set_style_radius(button, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, theme::line(), LV_PART_MAIN);
    lv_obj_set_style_text_color(button, theme::text(), LV_PART_MAIN);
    lv_obj_set_flex_flow(button, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(button, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    inherited_icon(button, symbol);
    return button;
}

lv_obj_t* small_label(lv_obj_t* parent, const lv_font_t* font, lv_color_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_remove_style_all(label);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
    lv_label_set_text(label, "");
    return label;
}

}  // namespace

void format_tenths(char* out, std::size_t size, int32_t tenths) {
    const int32_t whole = tenths / 10;
    const int32_t fraction = std::abs(tenths % 10);
    lv_snprintf(out, size, "%s%d,%d", tenths < 0 && whole == 0 ? "-" : "",
                static_cast<int>(whole), static_cast<int>(fraction));
}

void Thermostat::set_range(int32_t min_tenths, int32_t max_tenths, int32_t step_tenths) {
    if (max_tenths <= min_tenths || step_tenths <= 0) {
        return;  // encja podała bzdurę — zostajemy przy poprzednim zakresie
    }
    if (min_tenths == min_ && max_tenths == max_ && step_tenths == step_) {
        return;
    }
    min_ = min_tenths;
    max_ = max_tenths;
    step_ = step_tenths;
    lv_arc_set_range(arc, min_, max_);
    char text[16];
    format_tenths(text, sizeof(text), min_);
    lv_label_set_text_fmt(scale_min, "%s°", text);
    format_tenths(text, sizeof(text), max_);
    lv_label_set_text_fmt(scale_max, "%s°", text);
    set_value(value_);
}

void Thermostat::set_value(int32_t tenths) {
    value_ = std::clamp(tenths, min_, max_);
    lv_arc_set_value(arc, value_);
    char text[16];
    format_tenths(text, sizeof(text), value_);
    lv_label_set_text(value_label, text);
    lv_obj_set_style_opa(minus, enabled_ && value_ > min_ ? LV_OPA_COVER : 77, LV_PART_MAIN);
    lv_obj_set_style_opa(plus, enabled_ && value_ < max_ ? LV_OPA_COVER : 77, LV_PART_MAIN);
}

void Thermostat::set_status(domain::ControlStatus status) {
    lv_color_t color = theme::text();
    if (!enabled_) {
        color = theme::faint();
    } else if (status == domain::ControlStatus::Pending) {
        color = theme::accent();
    } else if (status == domain::ControlStatus::Failed) {
        color = theme::danger();
    }
    lv_obj_set_style_text_color(value_label, color, LV_PART_MAIN);
}

void Thermostat::set_enabled(bool enabled) {
    if (enabled == enabled_) {
        return;
    }
    enabled_ = enabled;
    if (!enabled) {
        thermo_stop_timer(this);
        moved_ = false;
    }
    // Zgaszona tarcza: bez wypełnienia i uchwytu, przyciski nie reagują.
    lv_obj_set_style_arc_opa(arc, enabled ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, enabled ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_opa(arc, enabled ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, enabled ? theme::surface3() : theme::surface2(), LV_PART_MAIN);
    for (lv_obj_t* obj : {ring, minus, plus}) {
        if (enabled) {
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        } else {
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_CLICKABLE);
        }
    }
    set_value(value_);
    set_status(domain::ControlStatus::Idle);
}

void Thermostat::set_texts(const char* action, const char* now) {
    lv_label_set_text(action_label, action);
    lv_label_set_text(now_label, now);
}

Thermostat* make_thermostat(lv_obj_t* parent, ValueHandler on_change, void* context) {
    auto* thermo = static_cast<Thermostat*>(lv_malloc_zeroed(sizeof(Thermostat)));
    thermo->min_ = 160;
    thermo->max_ = 300;
    thermo->step_ = 5;
    thermo->enabled_ = true;
    thermo->on_change = on_change;
    thermo->context = context;

    thermo->root = plain(parent);
    lv_obj_set_size(thermo->root, theme::kContentW, kThermoHeight);

    // Powierzchnia gestu leży nad łukiem, bo lv_arc sam w sobie skacze do palca.
    thermo->ring = plain(thermo->root);
    lv_obj_set_size(thermo->ring, kThermoSize, kThermoSize);
    lv_obj_align(thermo->ring, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_flag(thermo->ring, LV_OBJ_FLAG_CLICKABLE);

    thermo->arc = lv_arc_create(thermo->ring);
    lv_obj_remove_style_all(thermo->arc);
    lv_obj_set_size(thermo->arc, kThermoSize, kThermoSize);
    lv_obj_center(thermo->arc);
    lv_obj_remove_flag(thermo->arc, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_bg_angles(thermo->arc, 135, 45);
    lv_arc_set_range(thermo->arc, thermo->min_, thermo->max_);
    lv_obj_set_style_arc_width(thermo->arc, kThermoRing, LV_PART_MAIN);
    lv_obj_set_style_arc_color(thermo->arc, theme::surface3(), LV_PART_MAIN);
    lv_obj_set_style_arc_width(thermo->arc, kThermoRing, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(thermo->arc, theme::accent(), LV_PART_INDICATOR);
    // Uchwyt w miejscu bieżącej wartości: jasne koło z obwódką w kolorze tła.
    lv_obj_set_style_bg_opa(thermo->arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_bg_color(thermo->arc, theme::text(), LV_PART_KNOB);
    lv_obj_set_style_radius(thermo->arc, LV_RADIUS_CIRCLE, LV_PART_KNOB);
    lv_obj_set_style_pad_all(thermo->arc, kThermoKnobPad, LV_PART_KNOB);
    lv_obj_set_style_border_width(thermo->arc, 5, LV_PART_KNOB);
    lv_obj_set_style_border_color(thermo->arc, theme::bg(), LV_PART_KNOB);

    thermo->scale_min = small_label(thermo->ring, theme::font_r11(), theme::faint());
    lv_obj_align(thermo->scale_min, LV_ALIGN_BOTTOM_LEFT, 34, -22);
    thermo->scale_max = small_label(thermo->ring, theme::font_r11(), theme::faint());
    lv_obj_align(thermo->scale_max, LV_ALIGN_BOTTOM_RIGHT, -34, -22);

    lv_obj_t* readout = plain(thermo->ring);
    lv_obj_remove_flag(readout, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(readout, kThermoSize - 2 * kThermoRing - 16, LV_SIZE_CONTENT);
    lv_obj_center(readout);
    theme::make_column(readout, theme::kS3);
    lv_obj_set_flex_align(readout, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    thermo->action_label = make_cap(readout, "");

    lv_obj_t* value_row = plain(readout);
    lv_obj_remove_flag(value_row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(value_row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(value_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(value_row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(value_row, 2, LV_PART_MAIN);
    thermo->value_label = small_label(value_row, theme::font_b64num(), theme::text());
    thermo->unit_label = small_label(value_row, theme::font_b26(), theme::dim());
    lv_label_set_text(thermo->unit_label, "°C");

    thermo->now_label = small_label(readout, theme::font_r13(), theme::dim());

    thermo->minus = thermo_step_button(thermo->root, LV_SYMBOL_MINUS, -1);
    thermo->plus = thermo_step_button(thermo->root, LV_SYMBOL_PLUS, 1);
    lv_obj_add_event_cb(
        thermo->minus,
        [](lv_event_t* event) {
            thermo_nudge(static_cast<Thermostat*>(lv_event_get_user_data(event)), -1);
        },
        LV_EVENT_CLICKED, thermo);
    lv_obj_add_event_cb(
        thermo->plus,
        [](lv_event_t* event) {
            thermo_nudge(static_cast<Thermostat*>(lv_event_get_user_data(event)), 1);
        },
        LV_EVENT_CLICKED, thermo);

    lv_obj_add_event_cb(thermo->ring, thermo_event, LV_EVENT_PRESSED, thermo);
    lv_obj_add_event_cb(thermo->ring, thermo_event, LV_EVENT_PRESSING, thermo);
    lv_obj_add_event_cb(thermo->ring, thermo_event, LV_EVENT_RELEASED, thermo);
    lv_obj_add_event_cb(thermo->ring, thermo_event, LV_EVENT_PRESS_LOST, thermo);
    lv_obj_add_event_cb(
        thermo->root,
        [](lv_event_t* event) {
            auto* data = static_cast<Thermostat*>(lv_event_get_user_data(event));
            thermo_stop_timer(data);
            lv_free(data);
        },
        LV_EVENT_DELETE, thermo);

    char text[16];
    format_tenths(text, sizeof(text), thermo->min_);
    lv_label_set_text_fmt(thermo->scale_min, "%s°", text);
    format_tenths(text, sizeof(text), thermo->max_);
    lv_label_set_text_fmt(thermo->scale_max, "%s°", text);
    thermo->set_value(thermo->min_);
    return thermo;
}

// ============================= kolumna poziomu =============================

void Level::set_value(int32_t value) {
    value_ = clamp100(value);
    lv_bar_set_value(bar, value_, LV_ANIM_OFF);
    lv_label_set_text_fmt(value_label, "%d", static_cast<int>(value_));
}

void Level::set_status(domain::ControlStatus status) {
    lv_obj_set_style_text_color(
        value_label,
        status == domain::ControlStatus::Idle ? theme::text()
        : status == domain::ControlStatus::Pending ? theme::accent()
                                                   : theme::danger(),
        LV_PART_MAIN);
    set_status_outline(bar, status);
}

namespace {

void level_event(lv_event_t* event) {
    auto* level = static_cast<Level*>(lv_event_get_user_data(event));
    const lv_event_code_t code = lv_event_get_code(event);

    if (code == LV_EVENT_PRESSED) {
        level->accumulator_ = static_cast<float>(level->value_);
        level->moved_ = false;
        return;
    }

    if (code == LV_EVENT_PRESSING) {
        // lv_indev_get_vect() podaje przyrost ruchu palca — dokładnie to,
        // czego potrzebuje gest względny.
        lv_point_t vect{};
        lv_indev_get_vect(lv_indev_active(), &vect);
        if (vect.y == 0) {
            return;
        }
        const float delta = static_cast<float>(-vect.y) / static_cast<float>(kLevelTravel) * 100.0F;
        level->accumulator_ = std::clamp(level->accumulator_ + delta, 0.0F, 100.0F);
        const int32_t next = step2(static_cast<int32_t>(level->accumulator_));
        if (next != level->value_) {
            level->moved_ = true;
            level->set_value(next);
            level->set_status(domain::ControlStatus::Pending);
        }
        return;
    }

    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        if (level->moved_ && level->on_change != nullptr) {
            level->on_change(level->context, level->value_, true);
        }
        level->moved_ = false;
    }
}

}  // namespace

Level* make_level(lv_obj_t* parent, const char* name, ValueHandler on_change, void* context) {
    auto* level = static_cast<Level*>(lv_malloc_zeroed(sizeof(Level)));
    level->on_change = on_change;
    level->context = context;

    level->root = plain(parent);
    lv_obj_set_flex_grow(level->root, 1);
    lv_obj_set_height(level->root, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_ver(level->root, theme::kS2, LV_PART_MAIN);
    lv_obj_set_style_radius(level->root, theme::kR3, LV_PART_MAIN);
    theme::make_column(level->root, theme::kS2);
    lv_obj_set_flex_align(level->root, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(level->root, LV_OBJ_FLAG_CLICKABLE);

    level->name_label = make_cap(level->root, name);

    level->value_label = lv_label_create(level->root);
    lv_obj_remove_style_all(level->value_label);
    lv_obj_set_style_text_font(level->value_label, theme::font_b15(), LV_PART_MAIN);
    lv_obj_set_style_text_color(level->value_label, theme::text(), LV_PART_MAIN);
    lv_label_set_text(level->value_label, "0");

    level->bar = lv_bar_create(level->root);
    lv_obj_remove_style_all(level->bar);
    // lv_bar jest klikalny po lv_obj: zabierałby dotyk kolumnie, która prowadzi gest.
    lv_obj_remove_flag(level->bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(level->bar, kLevelTrackW, kLevelTrackH);
    lv_obj_set_style_bg_opa(level->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(level->bar, theme::surface2(), LV_PART_MAIN);
    lv_obj_set_style_radius(level->bar, theme::kR2, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(level->bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(level->bar, theme::accent(), LV_PART_INDICATOR);
    lv_obj_set_style_radius(level->bar, theme::kR2, LV_PART_INDICATOR);
    lv_bar_set_range(level->bar, 0, 100);
    lv_bar_set_value(level->bar, 0, LV_ANIM_OFF);

    lv_obj_add_event_cb(level->root, level_event, LV_EVENT_PRESSED, level);
    lv_obj_add_event_cb(level->root, level_event, LV_EVENT_PRESSING, level);
    lv_obj_add_event_cb(level->root, level_event, LV_EVENT_RELEASED, level);
    lv_obj_add_event_cb(level->root, level_event, LV_EVENT_PRESS_LOST, level);
    lv_obj_add_event_cb(
        level->root, [](lv_event_t* event) { lv_free(lv_event_get_user_data(event)); },
        LV_EVENT_DELETE, level);
    return level;
}

}  // namespace ui::widgets
