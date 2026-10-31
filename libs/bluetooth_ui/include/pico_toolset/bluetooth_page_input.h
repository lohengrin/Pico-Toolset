#pragma once

#include "pico_toolset/bluetooth_page_view.h"
#include "pico_toolset/hid_source.h"

namespace pico_toolset {

// A touch or pointer position in display pixels.
struct BtUiPointer {
    bool down = false;
    int  x = 0, y = 0;
};

// Which GPIO button does what (indices into the host's own button list, -1 = none).
struct BtUiButtonMap {
    int8_t prev = -1, next = -1, select = -1, back = -1;
};

struct BluetoothPageInputConfig {
    HidSource*    hid = nullptr;     // keyboard, mouse and gamepads (e.g. BluetoothHidSource, UsbHidSource)
    bool          has_touch = false;
    BtUiButtonMap buttons{};         // set at least one index to declare physical buttons
    // Holding a key / direction repeats it: first after `repeat_first` polls, then every `repeat_every`.
    uint16_t      repeat_first = 24;
    uint16_t      repeat_every = 6;
};

// Turns whatever input the host has into the page's abstract input and taps. Call poll() at a steady
// rate (about 60 Hz, the repeat is counted in polls). It also tells the view which inputs are present,
// so the controls become touch-sized or show a focus by themselves as devices come and go.
//
//   keyboard   arrows / Tab (Shift+Tab back) move, Enter or Space select, Esc or Backspace go back
//   gamepad    d-pad or left stick move, A selects, B or Back goes back
//   mouse      a click on a control; the wheel moves the focus; the app draws the cursor
//   touch      a tap (press and release on the same control)
//   buttons    mapped by BtUiButtonMap
class BluetoothPageInput {
public:
    explicit BluetoothPageInput(const BluetoothPageInputConfig& config) : cfg_(config) {}

    // `touch` is the current touch position (display pixels) or null; `button` is the button that
    // was just pressed (e.g. DebouncedButtons::poll()) or -1.
    void poll(BluetoothPageController& page, BluetoothPageView& view, const BtUiPointer* touch = nullptr,
              int button = -1);

    // The inputs present right now (bt_ui_input flags).
    [[nodiscard]] uint8_t capabilities() const { return caps_; }

private:
    struct Repeat {
        bool    down = false;
        uint16_t held = 0;
        bool fire(bool now, uint16_t first, uint16_t every);
    };
    struct Tap {
        bool down = false;
        int  item = -1;
        int  last_x = 0, last_y = 0;
        bool fire(BluetoothPageController& page, BluetoothPageView& view, const BtUiPointer& p);
    };

    void poll_keyboard(BluetoothPageController& page);
    void poll_pads(BluetoothPageController& page);
    void poll_mouse(BluetoothPageController& page, BluetoothPageView& view);

    BluetoothPageInputConfig cfg_;
    uint8_t caps_ = 0;
    Repeat key_prev_, key_next_, key_tab_, key_back_tab_, pad_prev_, pad_next_;
    bool key_select_ = false, key_back_ = false, pad_select_ = false, pad_back_ = false;
    Tap touch_tap_, mouse_tap_;
};

} // namespace pico_toolset
