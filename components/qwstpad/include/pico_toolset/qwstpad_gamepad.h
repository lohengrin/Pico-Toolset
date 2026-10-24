#pragma once

#include "pico_toolset/hid_gamepad.h"
#include "pico_toolset/qwstpad.h"

namespace pico_toolset {

// QwSTPad buttons (read_buttons(), qwstpad_button::*) as the toolset's unified GamepadState, so a
// pad on the QW/ST connector can stand in for, or be merged() with, a USB / Bluetooth gamepad:
// A/B/X/Y and the D-pad map one to one, `+` is Start and `-` is Back. Sticks stay centred.
inline GamepadState qwstpad_gamepad_state(uint16_t buttons) {
    GamepadState s;
    s.present = true;
    auto map = [&](uint16_t from, uint16_t to) { if (buttons & from) s.buttons |= to; };
    map(qwstpad_button::A, kBtA);
    map(qwstpad_button::B, kBtB);
    map(qwstpad_button::X, kBtX);
    map(qwstpad_button::Y, kBtY);
    map(qwstpad_button::Up, kBtUp);
    map(qwstpad_button::Down, kBtDown);
    map(qwstpad_button::Left, kBtLeft);
    map(qwstpad_button::Right, kBtRight);
    map(qwstpad_button::Plus, kBtStart);
    map(qwstpad_button::Minus, kBtBack);
    return s;
}

} // namespace pico_toolset
