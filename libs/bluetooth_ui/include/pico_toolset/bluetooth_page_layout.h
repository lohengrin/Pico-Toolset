#pragma once

#include "pico_toolset/bluetooth_page_controller.h"

namespace pico_toolset {

struct BtUiRect {
    int x = 0, y = 0, w = 0, h = 0;
    [[nodiscard]] bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }
};

// What the host has for input: decides the size of the controls and whether a focus is shown.
namespace bt_ui_input {
inline constexpr uint8_t Touch = 1, Pointer = 2, Keyboard = 4, Pad = 8, Buttons = 16;
inline constexpr uint8_t Direct = Touch | Pointer;       // can hit a control: needs big targets
inline constexpr uint8_t Navigate = Keyboard | Pad | Buttons;  // moves a focus
}

struct BtUiLayoutConfig {
    BtUiRect area;
    uint8_t  input_caps = bt_ui_input::Touch;
    int      min_target_px = 28;  // smallest control for touch / pointer
};

// Where everything goes for one frame. Pure arithmetic: no display needed, so it is tested on a PC.
struct BtUiLayout {
    int scale = 1;                 // title text scale (a glyph cell is 6*scale x 8*scale)
    int text_scale = 1;            // everything else: half the title's, at least 1
    int row_h = 0;
    int text_cols = 0;             // body characters that fit in a list row after the marker
    BtUiRect header, status, list, footer;
    bool status_in_header = false; // too small for a separate status line
    bool show_focus = false;
    size_t visible_rows = 0;
    size_t first_row = 0;          // scroll position, in rows
    bool more_above = false, more_below = false;
    BtUiRect item_rect[BluetoothPageController::kMaxItems];
    bool item_visible[BluetoothPageController::kMaxItems]{};
    size_t info_lines = 0;         // lines of text that fit in the list region on Detail / Confirm screens
};

[[nodiscard]] bool bt_ui_item_is_row(BtUiAction action);

// `scroll` is the first visible row from the previous frame; the result keeps the focused row in view.
void bt_ui_compute_layout(const BluetoothPageController& page, const BtUiLayoutConfig& config, size_t scroll,
                          BtUiLayout& out);

// The item under (x, y) or -1.
[[nodiscard]] int bt_ui_hit_test(const BtUiLayout& layout, size_t item_count, int x, int y);

} // namespace pico_toolset
