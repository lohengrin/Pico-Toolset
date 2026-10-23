#pragma once
// Pure HID report handling shared by the LE and Classic paths of bluetooth_hid: descriptor
// classification and input-report parsing. No BTstack stack state, so it is unit-testable on a PC
// (tests/host/, built against BTstack's own btstack_hid_parser.c).

#include <cstdint>

#include "pico_toolset/hid_gamepad.h"

namespace pico_toolset::hid_report {

// Report descriptor assumed for Classic pads reached through the raw L2CAP fallback (report id 3 =
// gamepad, see hid_report.cpp).
extern const uint8_t kRawPadDescriptor[];
extern const unsigned kRawPadDescriptorSize;

// Top-level application collections of a descriptor -> bt_hid_kind bits.
uint8_t scan_kinds(const uint8_t* descriptor, uint16_t len);

// Scales a HID field to 0-255 from its bit size and logical minimum.
uint8_t norm_axis(int32_t value, uint8_t size_bits, int32_t logical_min);

// What one input report carried. Axes are indexed lx, ly, rx, ry, lt, rt.
struct ParsedReport {
    bool has_keyboard = false;
    uint32_t key_bits[8] = {};
    uint8_t modifiers = 0;
    uint32_t raw_buttons = 0; // button usages 1..32, bit n-1 = usage n
    int hat = -1;             // 0..7 clockwise from up, -1 = centred / absent
    bool have[6] = {};
    uint8_t axis[6] = {};
    int dx = 0, dy = 0, wheel = 0; // only filled when `mouse_dev`
};

// `mouse_dev`: report X/Y as relative motion instead of stick axes. `report` starts after the
// HID DATA header and carries the report id first when the descriptor uses ids.
ParsedReport parse_input_report(const uint8_t* descriptor, uint16_t descriptor_len, const uint8_t* report,
                                uint16_t len, bool mouse_dev);

// Unified kBt* button mask for raw button usages (Xbox / Linux order) and a hat direction.
uint16_t map_buttons(uint32_t raw_buttons);
uint16_t hat_to_dpad(int hat);

} // namespace pico_toolset::hid_report
