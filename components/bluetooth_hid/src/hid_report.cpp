#include "hid_report.h"

#include "btstack_hid_parser.h"
#include "pico_toolset/bluetooth_hid.h"

namespace pico_toolset::hid_report {

namespace {
constexpr uint16_t kPageDesktop = 0x01, kPageSim = 0x02, kPageKeyboard = 0x07, kPageButton = 0x09;
} // namespace

// Report descriptor assumed for Classic pads reached through the raw L2CAP fallback (their SDP
// records are unusable). It is the descriptor read from the "SFR Gamepad" (vendor 0x2E2C): the
// common Android-style layout with report id 3 = gamepad (15 buttons, hat, X/Y/Z/Rz, brake,
// accelerator); ids 1, 2 and 4 are keyboard, mouse and media keys.
const uint8_t kRawPadDescriptor[] = {
    0x05, 0x01, 0x09, 0x06, 0xa1, 0x01, 0x85, 0x01, 0x75, 0x01, 0x95, 0x08,
    0x05, 0x07, 0x19, 0xe0, 0x29, 0xe7, 0x15, 0x00, 0x25, 0x01, 0x81, 0x02,
    0x95, 0x01, 0x75, 0x08, 0x81, 0x03, 0x95, 0x06, 0x75, 0x08, 0x15, 0x00,
    0x26, 0xef, 0x00, 0x05, 0x07, 0x19, 0x15, 0x29, 0xef, 0x81, 0x00, 0xc0,
    0x05, 0x01, 0x09, 0x02, 0xa1, 0x01, 0x85, 0x02, 0x09, 0x01, 0xa1, 0x00,
    0x05, 0x09, 0x19, 0x01, 0x29, 0x03, 0x15, 0x00, 0x25, 0x01, 0x95, 0x03,
    0x75, 0x01, 0x81, 0x02, 0x95, 0x01, 0x75, 0x05, 0x81, 0x03, 0x05, 0x01,
    0x09, 0x30, 0x09, 0x31, 0x09, 0x38, 0x15, 0x81, 0x25, 0x7f, 0x75, 0x08,
    0x95, 0x03, 0x81, 0x06, 0xc0, 0xc0, 0x05, 0x01, 0x09, 0x05, 0xa1, 0x01,
    0x85, 0x03, 0x15, 0x00, 0x25, 0x01, 0x35, 0x00, 0x45, 0x01, 0x75, 0x01,
    0x95, 0x0f, 0x05, 0x09, 0x19, 0x01, 0x29, 0x0f, 0x81, 0x02, 0x95, 0x01,
    0x81, 0x01, 0x05, 0x01, 0x25, 0x07, 0x46, 0x3b, 0x01, 0x75, 0x04, 0x95,
    0x01, 0x65, 0x14, 0x09, 0x39, 0x81, 0x42, 0x65, 0x00, 0x95, 0x01, 0x81,
    0x01, 0x26, 0xff, 0x00, 0x46, 0xff, 0x00, 0x09, 0x30, 0x09, 0x31, 0x09,
    0x32, 0x09, 0x35, 0x75, 0x08, 0x95, 0x04, 0x81, 0x02, 0x05, 0x02, 0x15,
    0x00, 0x26, 0xff, 0x00, 0x09, 0xc5, 0x09, 0xc4, 0x95, 0x02, 0x75, 0x08,
    0x81, 0x02, 0x95, 0x01, 0x75, 0x08, 0x81, 0x01, 0xa1, 0x02, 0x75, 0x08,
    0x95, 0x04, 0x15, 0x00, 0x35, 0x00, 0x46, 0xff, 0x00, 0x26, 0xff, 0x00,
    0x09, 0x02, 0x91, 0x02, 0xc0, 0xc0, 0x05, 0x0c, 0x09, 0x01, 0xa1, 0x01,
    0x85, 0x04, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08, 0x09, 0x40,
    0x0a, 0x23, 0x02, 0x0a, 0x24, 0x02, 0x09, 0xcd, 0x09, 0xb5, 0x09, 0x8f,
    0x09, 0xea, 0x09, 0xe9, 0x81, 0x02, 0x95, 0x28, 0x81, 0x03, 0xc0,
};
const unsigned kRawPadDescriptorSize = sizeof(kRawPadDescriptor);

// Scales a HID field to 0-255 using its bit size and logical minimum
// (assumes the field spans the full range of its size, true for the 8-bit
// and 16-bit sticks and the 10-bit triggers of common gamepads).
uint8_t norm_axis(int32_t value, uint8_t size_bits, int32_t logical_min) {
    int64_t v = static_cast<int64_t>(value) - logical_min;
    if (v < 0) v = 0;
    if (size_bits > 8) v >>= (size_bits - 8);
    else if (size_bits < 8) v <<= (8 - size_bits);
    return static_cast<uint8_t>(v > 255 ? 255 : v);
}

// HID button usage -> unified kBt* bit (Xbox-style layout; usage numbers
// follow the Linux gamepad order Xbox controllers use over Bluetooth). Verify
// against gamepad_raw_buttons() for other controllers.
struct ButtonMap { uint8_t usage; uint16_t mask; };
constexpr ButtonMap kButtonMap[] = {
    {1, kBtA}, {2, kBtB}, {4, kBtX}, {5, kBtY}, {7, kBtLB}, {8, kBtRB},
    {11, kBtBack}, {12, kBtStart}, {13, kBtGuide}, {14, kBtLS}, {15, kBtRS},
};

// Top-level application collections of a report descriptor -> kinds.
uint8_t scan_kinds(const uint8_t* descriptor, uint16_t len) {
    uint8_t kinds = 0;
    btstack_hid_descriptor_iterator_t it;
    btstack_hid_descriptor_iterator_init(&it, descriptor, len);
    uint16_t page = 0, usage = 0;
    int depth = 0;
    while (btstack_hid_descriptor_iterator_has_more(&it)) {
        const hid_descriptor_item_t* item = btstack_hid_descriptor_iterator_get_item(&it);
        if (item->item_type == Global && item->item_tag == UsagePage) {
            page = static_cast<uint16_t>(item->item_value);
        } else if (item->item_type == Local && item->item_tag == Usage) {
            usage = static_cast<uint16_t>(item->item_value);
        } else if (item->item_type == Main && item->item_tag == Coll) {
            if (depth == 0 && page == kPageDesktop) {
                if (usage == 0x06) kinds |= bt_hid_kind::Keyboard;
                else if (usage == 0x02) kinds |= bt_hid_kind::Mouse;
                else if (usage == 0x04 || usage == 0x05) kinds |= bt_hid_kind::Gamepad;
            }
            ++depth;
        } else if (item->item_type == Main && item->item_tag == EndColl) {
            if (depth > 0) --depth;
        }
        if (item->item_type == Main) usage = 0;
    }
    return kinds;
}

ParsedReport parse_input_report(const uint8_t* descriptor, uint16_t descriptor_len, const uint8_t* report,
                                uint16_t len, bool mouse_dev) {
    ParsedReport r;
    btstack_hid_parser_t parser;
    btstack_hid_parser_init(&parser, descriptor, descriptor_len, HID_REPORT_TYPE_INPUT, report, len);
    while (btstack_hid_parser_has_more(&parser)) {
        uint16_t page, usage;
        int32_t value;
        btstack_hid_parser_get_field(&parser, &page, &usage, &value);
        const uint8_t size = parser.descriptor_usage_item.size;
        const int32_t lmin = parser.descriptor_usage_item.global_logical_minimum;

        if (page == kPageKeyboard) {
            r.has_keyboard = true;
            if (usage >= 0xE0 && usage <= 0xE7) {
                if (value) r.modifiers |= static_cast<uint8_t>(1u << (usage - 0xE0));
            } else if (usage != 0 && usage < 256 && value) {
                r.key_bits[usage >> 5] |= 1u << (usage & 31);
            }
        } else if (page == kPageButton) {
            if (value && usage >= 1 && usage <= 32) r.raw_buttons |= 1u << (usage - 1);
        } else if (page == kPageDesktop) {
            switch (usage) {
            case 0x30:
                if (mouse_dev) r.dx = value; else { r.axis[0] = norm_axis(value, size, lmin); r.have[0] = true; }
                break;
            case 0x31:
                if (mouse_dev) r.dy = value; else { r.axis[1] = norm_axis(value, size, lmin); r.have[1] = true; }
                break;
            case 0x32: r.axis[2] = norm_axis(value, size, lmin); r.have[2] = true; break; // Z: right stick X
            case 0x35: r.axis[3] = norm_axis(value, size, lmin); r.have[3] = true; break; // Rz: right stick Y
            case 0x38: r.wheel = value; break;
            case 0x39: {
                const int h = value - lmin;
                r.hat = (h >= 0 && h <= 7) ? h : -1;
                break;
            }
            default: break;
            }
        } else if (page == kPageSim) {
            if (usage == 0xC5) { r.axis[4] = norm_axis(value, size, lmin); r.have[4] = true; } // brake: left trigger
            else if (usage == 0xC4) { r.axis[5] = norm_axis(value, size, lmin); r.have[5] = true; } // accelerator
        }
    }
    return r;
}

uint16_t map_buttons(uint32_t raw_buttons) {
    uint16_t b = 0;
    for (const auto& m : kButtonMap)
        if (raw_buttons & (1u << (m.usage - 1))) b |= m.mask;
    return b;
}

uint16_t hat_to_dpad(int hat) {
    uint16_t b = 0;
    if (hat < 0) return b;
    if (hat == 7 || hat == 0 || hat == 1) b |= kBtUp;
    if (hat >= 1 && hat <= 3) b |= kBtRight;
    if (hat >= 3 && hat <= 5) b |= kBtDown;
    if (hat >= 5 && hat <= 7) b |= kBtLeft;
    return b;
}

} // namespace pico_toolset::hid_report
