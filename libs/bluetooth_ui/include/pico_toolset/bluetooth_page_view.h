#pragma once

#include "pico_toolset/bluetooth_page_layout.h"
#include "pico_toolset/display_driver.h"

namespace pico_toolset {

struct BtUiTheme {
    Color background, text, dim, header, focus, button, button_text, ok, warn, bad;
};

inline constexpr BtUiTheme kBtUiDarkTheme{
    Color(0x0000), Color(0xFFFF), Color(0x8410), Color(0x2124), Color(0x2B7F),
    Color(0x3186), Color(0xFFFF), Color(0x07E0), Color(0xFFE0), Color(0xF800)};

struct BluetoothPageConfig {
    BtUiRect area;                     // where on the display the page lives (the whole display, or a pane)
    uint8_t  input_caps = bt_ui_input::Touch;
    BtUiTheme theme = kBtUiDarkTheme;
    const char* title = "Bluetooth";
};

// Draws a BluetoothPageController onto any DisplayDriver, sized to the area it is given: text scale,
// row height and the number of rows follow the area, controls are touch-sized when the host has touch
// or a pointer, and a focus highlight appears when it has a keyboard, pad or buttons. On 1-bit panels
// everything is white on black and the focus is shown inverted.
class BluetoothPageView {
public:
    BluetoothPageView(BluetoothPageController& page, const BluetoothPageConfig& config)
        : page_(page), cfg_(config) {}

    void set_input_caps(uint8_t caps) { cfg_.input_caps = caps; } // e.g. when a keyboard connects
    void set_area(const BtUiRect& area) { cfg_.area = area; }

    // Redraws the whole area. Call it when BluetoothPageController::update() returns true.
    void draw(DisplayDriver& display);

    // The item under a touch / pointer position, or -1; pass it to BluetoothPageController::activate().
    [[nodiscard]] int hit_test(int x, int y);

    [[nodiscard]] const BtUiLayout& layout() const { return layout_; }

private:
    void recompute();

    BluetoothPageController& page_;
    BluetoothPageConfig cfg_;
    BtUiLayout layout_{};
    size_t scroll_ = 0;
};

} // namespace pico_toolset
