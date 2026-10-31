#include "pico_toolset/bluetooth_page_lvgl.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#include "pico_toolset/bluetooth_page_text.h"

namespace pico_toolset {

namespace {

// Normal = the theme's own text colour (read from `from`), the others are fixed accents.
lv_color_t tone_color(BtUiTone tone, lv_obj_t* from) {
    switch (tone) {
    case BtUiTone::Dim: return lv_palette_main(LV_PALETTE_GREY);
    case BtUiTone::Ok: return lv_palette_main(LV_PALETTE_GREEN);
    case BtUiTone::Warn: return lv_palette_main(LV_PALETTE_YELLOW);
    case BtUiTone::Bad: return lv_palette_main(LV_PALETTE_RED);
    default: return lv_obj_get_style_text_color(from, LV_PART_MAIN);
    }
}

void set_text(lv_obj_t* label, const char* text) {
    if (std::strcmp(lv_label_get_text(label), text) != 0) lv_label_set_text(label, text);
}

void set_tone(lv_obj_t* label, BtUiTone tone, lv_obj_t* root) {
    lv_obj_set_style_text_color(label, tone_color(tone, root), 0);
}

struct Hash {
    uint32_t h = 2166136261u;
    void add(const void* p, size_t n) {
        for (size_t i = 0; i < n; ++i) h = (h ^ static_cast<const uint8_t*>(p)[i]) * 16777619u;
    }
    void add(const char* s) { add(s, std::strlen(s) + 1); }
    void add(uint32_t v) { add(&v, sizeof(v)); }
};

lv_obj_t* make_label(lv_obj_t* parent, const lv_font_t* font) {
    lv_obj_t* l = lv_label_create(parent);
    if (font) lv_obj_set_style_text_font(l, font, 0);
    return l;
}

} // namespace

lv_obj_t* BluetoothPageLvgl::create(lv_obj_t* parent) {
    destroy();
    group_ = lv_group_create();

    root_ = lv_obj_create(parent);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_border_width(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, cfg_.pad, 0);
    lv_obj_set_style_pad_row(root_, cfg_.pad / 2, 0);
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(root_, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_event_cb(root_, on_root_key, LV_EVENT_KEY, this);

    title_ = make_label(root_, cfg_.title_font);
    status_ = make_label(root_, cfg_.body_font);
    lv_obj_set_width(status_, lv_pct(100));
    lv_label_set_long_mode(status_, LV_LABEL_LONG_WRAP);

    passkey_ = make_label(root_, cfg_.passkey_font ? cfg_.passkey_font : cfg_.title_font);
    lv_obj_set_style_text_color(passkey_, tone_color(BtUiTone::Warn, root_), 0);
    lv_obj_set_style_text_letter_space(passkey_, 6, 0);
    lv_obj_set_style_align(passkey_, LV_ALIGN_CENTER, 0);
    lv_obj_add_flag(passkey_, LV_OBJ_FLAG_HIDDEN);

    list_ = lv_obj_create(root_);
    lv_obj_set_width(list_, lv_pct(100));
    lv_obj_set_flex_grow(list_, 1);
    lv_obj_add_flag(list_, LV_OBJ_FLAG_EVENT_BUBBLE); // keys reach the root
    lv_obj_set_flex_flow(list_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(list_, 4, 0);
    lv_obj_set_style_pad_row(list_, 4, 0);

    footer_ = lv_obj_create(root_);
    lv_obj_set_size(footer_, lv_pct(100), cfg_.button_height + 4);
    lv_obj_set_style_pad_all(footer_, 0, 0);
    lv_obj_set_style_border_width(footer_, 0, 0);
    lv_obj_set_style_bg_opa(footer_, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(footer_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(footer_, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_set_flex_flow(footer_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(footer_, cfg_.pad, 0);

    page_.update();
    signature_ = signature() + 1; // differs, so the first update() builds
    update();
    return root_;
}

void BluetoothPageLvgl::destroy() {
    if (root_) lv_obj_delete(root_);
    if (group_) lv_group_delete(group_);
    root_ = title_ = status_ = passkey_ = list_ = footer_ = nullptr;
    group_ = nullptr;
    std::memset(items_, 0, sizeof(items_));
}

void BluetoothPageLvgl::on_root_key(lv_event_t* e) {
    auto* self = static_cast<BluetoothPageLvgl*>(lv_event_get_user_data(e));
    const uint32_t key = lv_event_get_key(e);
    if (key == LV_KEY_ESC || key == LV_KEY_BACKSPACE) self->page_.handle(BtUiInput::Back);
}

void BluetoothPageLvgl::on_item_event(lv_event_t* e) {
    auto* self = static_cast<BluetoothPageLvgl*>(lv_event_get_user_data(e));
    const size_t item = reinterpret_cast<size_t>(lv_obj_get_user_data(lv_event_get_target_obj(e)));
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) self->page_.activate(item);
    else if (lv_event_get_code(e) == LV_EVENT_FOCUSED && !self->building_) self->page_.set_focus(item);
}

uint32_t BluetoothPageLvgl::signature() const {
    Hash h;
    h.add(static_cast<uint32_t>(page_.screen()));
    for (size_t i = 0; i < page_.item_count(); ++i) {
        const BtUiItem it = page_.item(i);
        h.add(static_cast<uint32_t>(it.action));
        if (it.action == BtUiAction::OpenDevice || it.action == BtUiAction::PickFound) {
            const BtUiDevice& d = it.action == BtUiAction::OpenDevice ? page_.bonded(static_cast<size_t>(it.index))
                                                                      : page_.found(static_cast<size_t>(it.index));
            char name[32], state[16];
            bt_ui_display_name(d, name, sizeof(name));
            bt_ui_row_state(it, d, state, sizeof(state));
            h.add(name);
            h.add(state);
            h.add(bt_ui_kind_tag(d.kind));
            h.add(static_cast<uint32_t>(bt_ui_row_state_tone(it, d)));
        } else {
            h.add(bt_ui_action_label(it.action, page_.screen(), false));
        }
    }
    BtUiText lines[3];
    const size_t n = bt_ui_info_lines(page_, lines, 3);
    for (size_t i = 0; i < n; ++i) { h.add(lines[i].text); h.add(static_cast<uint32_t>(lines[i].tone)); }
    return h.h;
}

void BluetoothPageLvgl::build_items() {
    building_ = true;
    lv_obj_clean(list_);
    lv_obj_clean(footer_);
    lv_group_remove_all_objs(group_);
    std::memset(items_, 0, sizeof(items_));

    BtUiText lines[3];
    const size_t line_count = bt_ui_info_lines(page_, lines, 3);
    for (size_t i = 0; i < line_count; ++i) {
        lv_obj_t* l = make_label(list_, cfg_.body_font);
        lv_label_set_text(l, lines[i].text);
        set_tone(l, lines[i].tone, root_);
    }

    for (size_t i = 0; i < page_.item_count(); ++i) {
        const BtUiItem it = page_.item(i);
        lv_obj_t* obj;
        if (it.action == BtUiAction::OpenDevice || it.action == BtUiAction::PickFound) {
            const BtUiDevice& d = it.action == BtUiAction::OpenDevice ? page_.bonded(static_cast<size_t>(it.index))
                                                                      : page_.found(static_cast<size_t>(it.index));
            obj = lv_obj_create(list_);
            lv_obj_set_size(obj, lv_pct(100), cfg_.row_height);
            lv_obj_set_style_pad_all(obj, 4, 0);
            lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(obj, LV_OBJ_FLAG_CLICKABLE);
            // The theme only highlights buttons: give a focused row (keyboard / pad) a visible mark.
            lv_obj_set_style_outline_width(obj, 3, LV_STATE_FOCUS_KEY);
            lv_obj_set_style_outline_color(obj, lv_palette_main(LV_PALETTE_BLUE), LV_STATE_FOCUS_KEY);
            lv_obj_set_style_outline_opa(obj, LV_OPA_COVER, LV_STATE_FOCUS_KEY);
            lv_obj_set_style_bg_color(obj, lv_palette_darken(LV_PALETTE_BLUE, 3), LV_STATE_FOCUS_KEY);
            lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_STATE_FOCUS_KEY);
            lv_obj_set_flex_flow(obj, LV_FLEX_FLOW_ROW);
            lv_obj_set_flex_align(obj, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
            lv_obj_set_style_pad_column(obj, cfg_.pad / 2, 0);

            char name[32], state[16];
            bt_ui_display_name(d, name, sizeof(name));
            bt_ui_row_state(it, d, state, sizeof(state));
            const BtUiTone tone = bt_ui_row_state_tone(it, d);

            lv_obj_t* tag = make_label(obj, cfg_.body_font);
            lv_label_set_text(tag, bt_ui_kind_tag(d.kind));
            lv_obj_set_width(tag, 56);
            lv_label_set_long_mode(tag, LV_LABEL_LONG_CLIP); // never wrap onto a second line
            set_tone(tag, BtUiTone::Dim, root_);
            lv_obj_t* nm = make_label(obj, cfg_.body_font);
            lv_label_set_text(nm, name);
            lv_label_set_long_mode(nm, LV_LABEL_LONG_DOT);
            lv_obj_set_flex_grow(nm, 1);
            if (it.action == BtUiAction::OpenDevice) set_tone(nm, tone, root_);
            if (state[0]) {
                lv_obj_t* st = make_label(obj, cfg_.body_font);
                lv_label_set_text(st, state);
                set_tone(st, tone == BtUiTone::Normal ? BtUiTone::Dim : tone, root_);
            }
        } else {
            obj = lv_button_create(footer_);
            lv_obj_set_height(obj, cfg_.button_height);
            lv_obj_set_flex_grow(obj, 1);
            lv_obj_t* l = make_label(obj, cfg_.body_font);
            lv_label_set_text(l, bt_ui_action_label(it.action, page_.screen(), false));
            lv_obj_center(l);
        }
        lv_obj_set_user_data(obj, reinterpret_cast<void*>(i));
        lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
        lv_obj_add_event_cb(obj, on_item_event, LV_EVENT_CLICKED, this);
        lv_obj_add_event_cb(obj, on_item_event, LV_EVENT_FOCUSED, this);
        lv_group_add_obj(group_, obj);
        items_[i] = obj;
    }
    building_ = false;
}

void BluetoothPageLvgl::refresh_header() {
    char buf[48];
    bt_ui_page_title(page_, cfg_.title, buf, sizeof(buf));
    set_text(title_, buf);

    const BtUiText st = bt_ui_status(page_);
    set_text(status_, st.text);
    set_tone(status_, st.tone, root_);

    uint32_t code = 0;
    if (page_.passkey(&code)) {
        std::snprintf(buf, sizeof(buf), "%06lu", static_cast<unsigned long>(code));
        set_text(passkey_, buf);
        lv_obj_remove_flag(passkey_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(passkey_, LV_OBJ_FLAG_HIDDEN);
    }
}

void BluetoothPageLvgl::sync_focus() {
    const size_t f = page_.focus();
    if (f < page_.item_count() && items_[f] && lv_group_get_focused(group_) != items_[f]) {
        building_ = true; // the controller already knows
        lv_group_focus_obj(items_[f]);
        building_ = false;
    }
}

bool BluetoothPageLvgl::update() {
    const bool changed = page_.update();
    if (!root_) return changed;
    refresh_header();
    const uint32_t sig = signature();
    if (sig != signature_) {
        signature_ = sig;
        build_items();
        sync_focus();
        return true;
    }
    if (changed) sync_focus();
    return changed;
}

} // namespace pico_toolset
