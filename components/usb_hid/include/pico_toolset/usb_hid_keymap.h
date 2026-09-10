#pragma once

#include <cstdint>

namespace pico_toolset {

// Maps a physical HID keyboard-page usage ID (letters/digits/punctuation in
// 0x04-0x38, the ISO extra key 0x64) to the printable ASCII character the
// selected layout prints for it, given whether Shift is held. Returns 0 for
// keys with no printable ASCII output (modifiers, F-keys, navigation, and
// non-ASCII legends like é/ù/£ -- see the FR keymap's notes).
using KeymapFn = uint8_t (*)(uint8_t usage_id, bool shift);

// One compiled-in layout. A Keymap's position in kKeymaps[] (the order its
// name appears in PICO_TOOLSET_USB_HID_KEYMAPS) is its stable runtime id.
struct Keymap {
    const char* name; // "us", "fr", ... -- what keymap_by_name() matches
    KeymapFn map;
};

// Number of keymaps compiled in (PICO_TOOLSET_USB_HID_KEYMAPS). The fallback
// value lets bare include-path consumers of this header still compile; the
// USB HID component always defines it via CMake.
#ifndef PICO_TOOLSET_USB_HID_KEYMAP_COUNT
#define PICO_TOOLSET_USB_HID_KEYMAP_COUNT 0
#endif
constexpr uint8_t kKeymapCount = PICO_TOOLSET_USB_HID_KEYMAP_COUNT;

// The compiled-in layouts, in PICO_TOOLSET_USB_HID_KEYMAPS order.
extern const Keymap kKeymaps[];

// Index into kKeymaps[] of the build-time default layout
// (PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP); used by hid_usage_to_ascii() when no
// keymap is given. Selected with UsbHidConfig::keymap_index.
#ifndef PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP_INDEX
#define PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP_INDEX 0
#endif
constexpr uint8_t kDefaultKeymapIndex = PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP_INDEX;

// The build-time default layout (PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP).
const Keymap& default_keymap();

// Lookup by name ("us", "fr", ...); nullptr when that keymap is not compiled
// in (i.e. not listed in PICO_TOOLSET_USB_HID_KEYMAPS).
const Keymap* keymap_by_name(const char* name);

// Boot-Protocol keyboard conversion. `modifier_mask` is the ModifierFlags
// byte (bit 1 LEFT_SHIFT, bit 5 RIGHT_SHIFT select the shifted symbol).
// The numeric keypad (0x54-0x63) is layout-independent and returned verbatim
// by every overload.
uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask);
uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask, uint8_t keymap_index);
uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask, const Keymap& keymap);

} // namespace pico_toolset