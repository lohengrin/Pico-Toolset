#include "pico_toolset/usb_hid_keymap.h"

#include <cstring>

namespace pico_toolset {

namespace {

// Keyboard-page usage IDs are *physical positions* (the HID spec's "USB
// keyboard page"), identical on every layout -- each layout table below just
// says which character a position prints. Two positions never present on a
// US-ANSI board matter for PC 105 (ISO) keyboards: 0x32 (Non-US # and ~) and
// 0x64 (the extra key left of Z).
//
// Both tables below cover every PC 105 key that can produce printable ASCII:
// letters, digits, punctuation, Enter/Esc/Backspace/Tab/Space and the keypad
// (the keypad is handled outside the tables -- it is layout-independent).

// --- US QWERTY (ANSI + ISO/PC 105) ---

uint8_t map_us(uint8_t usage_id, bool shift) {
    if (usage_id >= 0x04 && usage_id <= 0x1D) { // letters a-z
        uint8_t c = static_cast<uint8_t>('a' + (usage_id - 0x04));
        if (shift) c = static_cast<uint8_t>(c - 'a' + 'A');
        return c;
    }
    if (usage_id >= 0x1E && usage_id <= 0x27) { // digit row
        static constexpr char kPlain[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
        static constexpr char kShifted[10] = {'!', '@', '#', '$', '%', '^', '&', '*', '(', ')'};
        return shift ? kShifted[usage_id - 0x1E] : kPlain[usage_id - 0x1E];
    }
    switch (usage_id) {
        case 0x28: return '\n';   // Enter (keypad Enter handled elsewhere)
        case 0x29: return 0x1B;   // Escape
        case 0x2A: return 0x08;   // Backspace
        case 0x2B: return '\t';   // Tab
        case 0x2C: return ' ';    // Space
        case 0x2D: return shift ? '_' : '-';
        case 0x2E: return shift ? '+' : '=';
        case 0x2F: return shift ? '{' : '[';
        case 0x30: return shift ? '}' : ']';
        case 0x31: return shift ? '|' : '\\';  // ANSI backslash; PC 105 ISO keeps \ on 0x64
        case 0x32: return shift ? '~' : '#';   // Non-US # and ~ (ISO layouts)
        case 0x33: return shift ? ':' : ';';
        case 0x34: return shift ? '"' : '\'';
        case 0x35: return shift ? '~' : '`';   // Grave / tilde
        case 0x36: return shift ? '<' : ',';
        case 0x37: return shift ? '>' : '.';
        case 0x38: return shift ? '?' : '/';
        case 0x64: return shift ? '|' : '\\';  // Non-US backslash (PC 105/ISO extra key)
        default: return 0;
    }
}

// --- French AZERTY (PC 105 / ISO) ---
//
// Same physical positions as US; the keycaps at some of them differ, which is
// all this table expresses. Non-ASCII legends (é è ç à ù, ², °, £, µ, ^/¨ dead
// keys) have no ASCII output and return 0 -- they stay untypeable through this
// API, matching the ASCII-only contract of hid_usage_to_ascii().

uint8_t map_fr(uint8_t usage_id, bool shift) {
    // Letters moved from their US positions by AZERTY.
    switch (usage_id) {
        case 0x04: return shift ? 'Q' : 'q';  // US "A" position -> q
        case 0x14: return shift ? 'A' : 'a';  // US "Q" position -> a
        case 0x1A: return shift ? 'Z' : 'z';  // US "W" position -> z
        case 0x1D: return shift ? 'W' : 'w';  // US "Z" position -> w
        case 0x10: return shift ? '?' : ',';  // US "M" position -> ,/?
        case 0x33: return shift ? 0 : 'm';    // US ";" position -> m
        case 0x34: return shift ? '%' : 0;    // US "'" position -> ù (no ASCII)
        case 0x64: return shift ? '>' : '<';  // ISO extra key (PC 105) -> </
        default: break;
    }
    // Every other letter plugs into the same position as US (e, r, t, y, u,
    // i, o, p, s, d, f, g, h, j, k, l, b, c, v, n, x).
    if (usage_id >= 0x04 && usage_id <= 0x1D) {
        uint8_t c = static_cast<uint8_t>('a' + (usage_id - 0x04));
        if (shift) c = static_cast<uint8_t>(c - 'a' + 'A');
        return c;
    }
    // Digit row: symbols unshifted, digits shifted -- the reverse of US.
    // é/è/ç/à (non-ASCII) are 0.
    if (usage_id >= 0x1E && usage_id <= 0x27) {
        static constexpr char kPlain[10] = {'&', 0, '"', '\'', '(', '-', 0, '_', 0, 0};
        static constexpr char kShifted[10] = {'1', '2', '3', '4', '5', '6', '7', '8', '9', '0'};
        return shift ? kShifted[usage_id - 0x1E] : kPlain[usage_id - 0x1E];
    }
    switch (usage_id) {
        case 0x28: return '\n';
        case 0x29: return 0x1B;
        case 0x2A: return 0x08;
        case 0x2B: return '\t';
        case 0x2C: return ' ';
        case 0x2D: return shift ? 0 : ')';   // ) ; shifted "°" has no ASCII
        case 0x2E: return shift ? '+' : '=';
        case 0x2F: return 0;                 // ^ / ¨ (dead keys, no ASCII)
        case 0x30: return shift ? 0 : '$';   // shifted "£" has no ASCII
        case 0x31: return shift ? 0 : '*';   // shifted "µ" has no ASCII
        case 0x36: return shift ? '.' : ';';
        case 0x37: return shift ? '/' : ':';
        case 0x38: return shift ? 0 : '!';
        case 0x32: return 0;                 // not a physical key on AZERTY (ISO "Non-US #")
        case 0x35: return 0;                 // "²" has no ASCII
        default: return 0;
    }
}

} // namespace

// Each keymap is compiled in only when selected by PICO_TOOLSET_USB_HID_KEYMAPS
// (components/usb_hid/CMakeLists.txt turns it into a *_US/_FR =1 define and
// keeps the entry order identical to the CMake list).

const Keymap kKeymaps[] = {
#if PICO_TOOLSET_USB_HID_KEYMAP_US
    {"us", map_us},
#endif
#if PICO_TOOLSET_USB_HID_KEYMAP_FR
    {"fr", map_fr},
#endif
};

static_assert(PICO_TOOLSET_USB_HID_KEYMAP_COUNT ==
                  sizeof(kKeymaps) / sizeof(kKeymaps[0]),
              "PICO_TOOLSET_USB_HID_KEYMAP_COUNT must equal the entries compiled "
              "into pico_toolset::kKeymaps -- when adding a keymap, register its "
              "'#if PICO_TOOLSET_USB_HID_KEYMAP_*' block here and its name in "
              "PICO_TOOLSET_USB_HID_KEYMAPS in components/usb_hid/CMakeLists.txt");

const Keymap& default_keymap() { return kKeymaps[kDefaultKeymapIndex]; }

const Keymap* keymap_by_name(const char* name) {
    if (name == nullptr) return nullptr;
    for (const Keymap& km : kKeymaps)
        if (std::strcmp(km.name, name) == 0) return &km;
    return nullptr;
}

uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask) {
    return hid_usage_to_ascii(usage_id, modifier_mask, kDefaultKeymapIndex);
}

uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask, uint8_t keymap_index) {
    if (keymap_index >= kKeymapCount) keymap_index = kDefaultKeymapIndex;
    return hid_usage_to_ascii(usage_id, modifier_mask, kKeymaps[keymap_index]);
}

uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask, const Keymap& keymap) {
    // Numeric keypad: same character on every layout and regardless of Shift
    // (NumLock-off navigation meaning is not distinguished -- no way to read
    // the NumLock output-report state back, same convention as TOM6809).
    // 0x54..0x63: / * - + Enter 1 2 3 4 5 6 7 8 9 0 .
    if (usage_id >= 0x54 && usage_id <= 0x63) {
        static constexpr char kKeypad[16] = {'/', '*', '-', '+', '\n',
                                             '1', '2', '3', '4', '5',
                                             '6', '7', '8', '9', '0', '.'};
        return kKeypad[usage_id - 0x54];
    }
    if (keymap.map == nullptr) return 0;
    bool shift = (modifier_mask & (0x02 | 0x20)) != 0; // left shift | right shift
    return keymap.map(usage_id, shift);
}

} // namespace pico_toolset