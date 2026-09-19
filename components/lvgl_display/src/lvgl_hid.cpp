#include "pico_toolset/lvgl_hid.h"

namespace pico_toolset {

namespace {

void keypad_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* usb = static_cast<UsbHidHost*>(lv_indev_get_user_data(indev));
    uint32_t key = 0;

    // Arrows navigate via PREV/NEXT: LVGL only moves group focus on those;
    // raw UP/DOWN would scroll the parent list instead.
    if (usb->is_key_down(0x52)) key = LV_KEY_PREV;       // Up
    else if (usb->is_key_down(0x51)) key = LV_KEY_NEXT;  // Down
    else if (usb->is_key_down(0x50)) key = LV_KEY_PREV;  // Left
    else if (usb->is_key_down(0x4F)) key = LV_KEY_NEXT;  // Right
    else if (usb->is_key_down(0x28)) key = LV_KEY_ENTER; // Enter
    else if (usb->is_key_down(0x29)) key = LV_KEY_ESC;   // Esc
    else if (usb->is_key_down(0x2B)) key = LV_KEY_NEXT;  // Tab

    if (key == 0) {
        const GamepadState pad = usb->gamepad_state(0);
        if (pad.present) {
            if (pad.down(kBtUp) || pad.down(kBtLeft)) key = LV_KEY_PREV;
            else if (pad.down(kBtDown) || pad.down(kBtRight)) key = LV_KEY_NEXT;
            else if (pad.down(kBtA) || pad.down(kBtStart)) key = LV_KEY_ENTER;
            else if (pad.down(kBtB)) key = LV_KEY_ESC;
        }
    }

    data->key = key;
    data->state = key != 0 ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void mouse_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* usb = static_cast<UsbHidHost*>(lv_indev_get_user_data(indev));
    const UsbHidHost::MouseState mouse = usb->mouse_state();
    if (!mouse.present) {
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }
    data->point.x = mouse.x;
    data->point.y = mouse.y;
    data->state = (mouse.left_button || mouse.right_button || mouse.middle_button) ? LV_INDEV_STATE_PRESSED
                                                                                    : LV_INDEV_STATE_RELEASED;
}

} // namespace

void lvgl_hid_init(UsbHidHost& hid) {
    lv_group_t* group = lv_group_create();
    lv_group_set_default(group);

    lv_indev_t* keypad = lv_indev_create();
    lv_indev_set_type(keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(keypad, keypad_read_cb);
    lv_indev_set_user_data(keypad, &hid);
    lv_indev_set_group(keypad, group);

    lv_indev_t* mouse = lv_indev_create();
    lv_indev_set_type(mouse, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(mouse, mouse_read_cb);
    lv_indev_set_user_data(mouse, &hid);

    // Crosshair cursor, centred on the hit point (LVGL anchors a cursor's
    // top-left at the pointer).
    lv_obj_t* cursor = lv_label_create(lv_layer_top());
    lv_label_set_text(cursor, "+");
    lv_obj_set_style_text_color(cursor, lv_color_white(), 0);
    lv_obj_set_style_text_font(cursor, &lv_font_montserrat_20, 0);
    lv_obj_update_layout(cursor);
    lv_obj_set_style_translate_x(cursor, -(lv_obj_get_width(cursor) / 2), 0);
    lv_obj_set_style_translate_y(cursor, -(lv_obj_get_height(cursor) / 2), 0);
    lv_indev_set_cursor(mouse, cursor);
}

} // namespace pico_toolset
