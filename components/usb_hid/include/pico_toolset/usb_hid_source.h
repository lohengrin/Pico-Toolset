#pragma once

#include "pico_toolset/hid_source.h"
#include "pico_toolset/usb_hid_host.h"

namespace pico_toolset {

// HidSource over a UsbHidHost (which must outlive it).
class UsbHidSource final : public HidSource {
public:
    explicit UsbHidSource(UsbHidHost& host) : m_host(host) {}

    bool is_key_down(uint8_t usage) const override { return m_host.is_key_down(usage); }
    bool is_modifier_down(uint8_t mask) const override { return m_host.is_modifier_down(mask); }

    HidMouse mouse_state() const override {
        const UsbHidHost::MouseState m = m_host.mouse_state();
        HidMouse out;
        out.present = m.present;
        out.x = m.x;
        out.y = m.y;
        out.left = m.left_button;
        out.right = m.right_button;
        out.middle = m.middle_button;
        return out;
    }
    void consume_mouse_delta(int& dx, int& dy) override { m_host.consume_mouse_delta(dx, dy); }
    int consume_mouse_wheel() override { return m_host.consume_mouse_wheel(); }

    GamepadState gamepad_state(size_t index) const override { return m_host.gamepad_state(index); }
    uint8_t connected_keyboard_count() const override { return m_host.connected_keyboard_count(); }

private:
    UsbHidHost& m_host;
};

} // namespace pico_toolset
