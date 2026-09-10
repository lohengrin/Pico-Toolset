#pragma once

#include <cstdint>
#include <cstddef>
#include <array>

namespace pico_toolset {

// Unified gamepad state shared by HID gamepads and XInput pads. Button bits
// match the classic SNES+X-box style layout; every consumer gets the same
// representation whatever the physical device.
enum : uint16_t {
    kBtA      = 1u << 0,
    kBtB      = 1u << 1,
    kBtX      = 1u << 2,
    kBtY      = 1u << 3,
    kBtLB     = 1u << 4,
    kBtRB     = 1u << 5,
    kBtBack   = 1u << 6,
    kBtStart  = 1u << 7,
    kBtLS     = 1u << 8,
    kBtRS     = 1u << 9,
    kBtUp     = 1u << 10,
    kBtDown   = 1u << 11,
    kBtLeft   = 1u << 12,
    kBtRight  = 1u << 13,
    kBtGuide  = 1u << 14,
};

// kMaxGamepads total devices reported by the host (HID + XInput merged).
inline constexpr size_t kMaxGamepads = 4;

struct GamepadState {
    bool present = false;

    // Duplicate of kBt* -- a raw bitmask in case callers prefer bit tests.
    uint16_t buttons = 0;

    // Axes in 0-255, centered at 128 when centered.
    uint8_t lx = 128, ly = 128;
    uint8_t rx = 128, ry = 128;
    uint8_t lt = 0, rt = 0;

    bool is_xinput = false; // set for XInput pads

    bool down(uint16_t mask) const { return (buttons & mask) != 0; }
    bool pressed(uint16_t mask) const { return (buttons & mask) != 0; }
};

} // namespace pico_toolset