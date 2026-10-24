#pragma once

#include <cstddef>
#include <cstdint>

#include "pico_toolset/hid_gamepad.h"

namespace pico_toolset {

// Transport-neutral view of the attached HID input (keyboard / mouse / gamepad), so UI and game
// code written once works with a USB host (`UsbHidSource`, usb_hid_source.h), a Bluetooth host
// (`BluetoothHidSource`, bluetooth_hid_source.h) or an application-defined merge of several
// (e.g. a Bluetooth keyboard plus a QW/ST gamepad). The LVGL bridge (lvgl_hid.h) takes one.

struct HidMouse {
    bool present = false;
    int x = 0, y = 0; // absolute cursor, clamped by the source (menu / pointer use)
    bool left = false, right = false, middle = false;
    bool back = false, forward = false; // thumb buttons, where the transport reports them
};

class HidSource {
public:
    virtual ~HidSource() = default;

    // HID usage IDs (Keyboard/Keypad page); the modifier mask is the boot-protocol byte
    // (bit 0 L-Ctrl .. bit 7 R-GUI).
    [[nodiscard]] virtual bool is_key_down(uint8_t usage) const = 0;
    [[nodiscard]] virtual bool is_modifier_down(uint8_t mask) const = 0;

    [[nodiscard]] virtual HidMouse mouse_state() const = 0;
    // Raw relative motion / wheel detents since the last call (read-and-clear), for mouselook.
    virtual void consume_mouse_delta(int& dx, int& dy) = 0;
    virtual int consume_mouse_wheel() = 0;

    [[nodiscard]] virtual GamepadState gamepad_state(size_t index) const = 0;
    [[nodiscard]] virtual uint8_t connected_keyboard_count() const = 0;
};

} // namespace pico_toolset
