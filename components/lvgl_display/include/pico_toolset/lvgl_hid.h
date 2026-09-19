#pragma once

#include "pico_toolset/usb_hid_host.h"

#include "lvgl.h"

namespace pico_toolset {

// LVGL input devices for a USB HID host (keyboard, mouse, gamepad):
//  - keypad: arrows / Tab / Enter / Esc (keyboard) and D-pad / A / B / Start
//    (gamepad) drive LVGL's focus navigation on a default lv_group;
//  - pointer: mouse with a visible cursor, in canvas coordinates (configure
//    UsbHidConfig::mouse_max_x/y to canvas size - 1 so no scaling is needed).
// Creates a new lv_group and makes it the default: call BEFORE building the
// widgets that should be keyboard-navigable.
void lvgl_hid_init(UsbHidHost& hid);

} // namespace pico_toolset
