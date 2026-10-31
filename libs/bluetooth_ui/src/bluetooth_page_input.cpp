#include "pico_toolset/bluetooth_page_input.h"

namespace pico_toolset {

namespace {
constexpr uint8_t kUsageEnter = 0x28, kUsageEsc = 0x29, kUsageBackspace = 0x2A, kUsageTab = 0x2B, kUsageSpace = 0x2C,
                  kUsageRight = 0x4F, kUsageLeft = 0x50, kUsageDown = 0x51, kUsageUp = 0x52, kUsageKeypadEnter = 0x58;
constexpr uint8_t kShiftMask = 0x02 | 0x20;  // L-Shift, R-Shift
constexpr uint8_t kStickLow = 64, kStickHigh = 192;
}

bool BluetoothPageInput::Repeat::fire(bool now, uint16_t first, uint16_t every) {
    if (!now) { down = false; held = 0; return false; }
    if (!down) { down = true; held = 0; return true; }
    ++held;
    if (held < first) return false;
    if (every == 0 || (held - first) % every != 0) return false;
    return true;
}

// Press and release on the same control is a tap; pressing already moves the focus there. Lifting
// somewhere else (the last position seen while down) cancels it.
bool BluetoothPageInput::Tap::fire(BluetoothPageController& page, BluetoothPageView& view, const BtUiPointer& p) {
    if (p.down) {
        last_x = p.x;
        last_y = p.y;
        if (!down) {
            down = true;
            item = view.hit_test(p.x, p.y);
            if (item >= 0) page.set_focus(static_cast<size_t>(item));
        }
        return false;
    }
    if (!down) return false;
    down = false;
    const int started = item;
    item = -1;
    return started >= 0 && view.hit_test(last_x, last_y) == started;
}

void BluetoothPageInput::poll_keyboard(BluetoothPageController& page) {
    HidSource& h = *cfg_.hid;
    if (h.connected_keyboard_count() == 0) {
        key_prev_ = key_next_ = key_tab_ = key_back_tab_ = Repeat{};
        key_select_ = key_back_ = false;
        return;
    }
    caps_ |= bt_ui_input::Keyboard;
    const bool shift = h.is_modifier_down(kShiftMask);
    const bool up = h.is_key_down(kUsageUp) || h.is_key_down(kUsageLeft);
    const bool down = h.is_key_down(kUsageDown) || h.is_key_down(kUsageRight);
    const bool tab = h.is_key_down(kUsageTab);
    if (key_prev_.fire(up, cfg_.repeat_first, cfg_.repeat_every)) page.handle(BtUiInput::Prev);
    if (key_next_.fire(down, cfg_.repeat_first, cfg_.repeat_every)) page.handle(BtUiInput::Next);
    if (key_tab_.fire(tab && !shift, cfg_.repeat_first, cfg_.repeat_every)) page.handle(BtUiInput::Next);
    if (key_back_tab_.fire(tab && shift, cfg_.repeat_first, cfg_.repeat_every)) page.handle(BtUiInput::Prev);

    const bool select = h.is_key_down(kUsageEnter) || h.is_key_down(kUsageKeypadEnter) || h.is_key_down(kUsageSpace);
    const bool back = h.is_key_down(kUsageEsc) || h.is_key_down(kUsageBackspace);
    if (select && !key_select_) page.handle(BtUiInput::Select);
    if (back && !key_back_) page.handle(BtUiInput::Back);
    key_select_ = select;
    key_back_ = back;
}

void BluetoothPageInput::poll_pads(BluetoothPageController& page) {
    bool up = false, down = false, select = false, back = false, present = false;
    for (size_t i = 0; i < kMaxGamepads; ++i) {
        const GamepadState g = cfg_.hid->gamepad_state(i);
        if (!g.present) continue;
        present = true;
        up |= g.down(kBtUp) || g.down(kBtLeft) || g.ly < kStickLow || g.lx < kStickLow;
        down |= g.down(kBtDown) || g.down(kBtRight) || g.ly > kStickHigh || g.lx > kStickHigh;
        select |= g.down(kBtA);
        back |= g.down(kBtB) || g.down(kBtBack);
    }
    if (!present) {
        pad_prev_ = pad_next_ = Repeat{};
        pad_select_ = pad_back_ = false;
        return;
    }
    caps_ |= bt_ui_input::Pad;
    if (pad_prev_.fire(up, cfg_.repeat_first, cfg_.repeat_every)) page.handle(BtUiInput::Prev);
    if (pad_next_.fire(down, cfg_.repeat_first, cfg_.repeat_every)) page.handle(BtUiInput::Next);
    if (select && !pad_select_) page.handle(BtUiInput::Select);
    if (back && !pad_back_) page.handle(BtUiInput::Back);
    pad_select_ = select;
    pad_back_ = back;
}

void BluetoothPageInput::poll_mouse(BluetoothPageController& page, BluetoothPageView& view) {
    const HidMouse m = cfg_.hid->mouse_state();
    if (!m.present) { mouse_tap_ = Tap{}; return; }
    caps_ |= bt_ui_input::Pointer;
    BtUiPointer p;
    p.down = m.left;
    p.x = m.x;
    p.y = m.y;
    if (mouse_tap_.fire(page, view, p)) page.activate(static_cast<size_t>(page.focus()));
    const int wheel = cfg_.hid->consume_mouse_wheel();
    if (wheel > 0) page.handle(BtUiInput::Prev);
    else if (wheel < 0) page.handle(BtUiInput::Next);
}

void BluetoothPageInput::poll(BluetoothPageController& page, BluetoothPageView& view, const BtUiPointer* touch,
                              int button) {
    caps_ = 0;
    if (cfg_.has_touch) caps_ |= bt_ui_input::Touch;
    const BtUiButtonMap& b = cfg_.buttons;
    if (b.prev >= 0 || b.next >= 0 || b.select >= 0 || b.back >= 0) caps_ |= bt_ui_input::Buttons;
    if (cfg_.hid) {
        poll_keyboard(page);
        poll_pads(page);
        view.set_input_caps(caps_);   // before hit-testing: the target size depends on it
        poll_mouse(page, view);
    } else {
        view.set_input_caps(caps_);
    }
    if (touch && touch_tap_.fire(page, view, *touch)) page.activate(page.focus());
    else if (!touch) touch_tap_ = Tap{};

    if (button >= 0) {
        if (button == b.prev) page.handle(BtUiInput::Prev);
        else if (button == b.next) page.handle(BtUiInput::Next);
        else if (button == b.select) page.handle(BtUiInput::Select);
        else if (button == b.back) page.handle(BtUiInput::Back);
    }
}

} // namespace pico_toolset
