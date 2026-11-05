#pragma once
// Where a USB mouse puts its buttons, X, Y and wheel in an input report, read from its HID report
// descriptor. The boot protocol is only 3 bytes (buttons, dx, dy): a wheel needs the report protocol,
// and there the layout (report id, 8/12/16-bit axes) differs per mouse. Pure code, tested on a PC.

#include <cstdint>

namespace pico_toolset {

struct UsbMouseLayout {
    bool     valid = false;       // X and Y were found in a Mouse application collection
    bool     has_wheel = false;
    uint8_t  report_id = 0;       // 0 = the reports carry no id byte
    uint16_t buttons_bit = 0;     // first button, bit offset after the id byte
    uint8_t  buttons_count = 0;
    uint16_t x_bit = 0, y_bit = 0, wheel_bit = 0;
    uint8_t  x_size = 0, y_size = 0, wheel_size = 0; // bits
    bool     x_signed = true, y_signed = true, wheel_signed = true;
    uint16_t report_bytes = 0;    // length a complete report needs (id byte included)
};

struct UsbMouseReport {
    uint8_t buttons = 0;          // bit0 left, bit1 right, bit2 middle
    int dx = 0, dy = 0, wheel = 0; // wheel: positive = up / away from the user
};

[[nodiscard]] bool usb_mouse_parse_descriptor(const uint8_t* descriptor, uint16_t len, UsbMouseLayout& out);

// False when the report is not the mouse's (other report id) or too short.
[[nodiscard]] bool usb_mouse_parse_report(const UsbMouseLayout& layout, const uint8_t* report, uint16_t len,
                                          UsbMouseReport& out);

} // namespace pico_toolset
