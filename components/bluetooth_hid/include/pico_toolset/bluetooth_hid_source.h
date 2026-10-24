#pragma once

#include <algorithm>

#include "pico_toolset/bluetooth_hid.h"
#include "pico_toolset/hid_source.h"

namespace pico_toolset {

// HidSource over a BluetoothHid (which must outlive it). Bluetooth mice report relative motion
// only, so this keeps a virtual cursor clamped to [0, max_x] x [0, max_y] for `mouse_state()` and
// a separate raw accumulator for `consume_mouse_delta()`: call poll() regularly from one thread
// (the main loop) to fold the host's motion into both; they are then independent consumers.
class BluetoothHidSource final : public HidSource {
public:
    BluetoothHidSource(BluetoothHid& host, int max_x = 639, int max_y = 479)
        : m_host(host), m_max_x(max_x), m_max_y(max_y), m_x(max_x / 2), m_y(max_y / 2) {}

    void poll() {
        int dx = 0, dy = 0;
        m_host.consume_mouse_delta(dx, dy);
        if (dx != 0 || dy != 0) {
            m_acc_dx += dx;
            m_acc_dy += dy;
            m_x = std::clamp(m_x + dx, 0, m_max_x);
            m_y = std::clamp(m_y + dy, 0, m_max_y);
        }
        m_acc_wheel += m_host.consume_mouse_wheel();
    }

    bool is_key_down(uint8_t usage) const override { return m_host.is_key_down(usage); }
    bool is_modifier_down(uint8_t mask) const override { return m_host.is_modifier_down(mask); }

    HidMouse mouse_state() const override {
        const BluetoothHid::MouseState m = m_host.mouse_state();
        HidMouse out;
        out.present = m.present;
        out.x = m_x;
        out.y = m_y;
        out.left = m.left;
        out.right = m.right;
        out.middle = m.middle;
        out.back = m.back;
        out.forward = m.forward;
        return out;
    }
    void consume_mouse_delta(int& dx, int& dy) override {
        dx = m_acc_dx;
        dy = m_acc_dy;
        m_acc_dx = m_acc_dy = 0;
    }
    int consume_mouse_wheel() override {
        const int w = m_acc_wheel;
        m_acc_wheel = 0;
        return w;
    }

    GamepadState gamepad_state(size_t index) const override { return m_host.gamepad_state(index); }
    uint8_t connected_keyboard_count() const override { return m_host.connected_keyboard_count(); }

private:
    BluetoothHid& m_host;
    int m_max_x, m_max_y;
    int m_x, m_y;
    int m_acc_dx = 0, m_acc_dy = 0, m_acc_wheel = 0;
};

} // namespace pico_toolset
