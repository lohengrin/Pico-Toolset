#pragma once

#include <cstddef>

#include "pico_toolset/bluetooth_page_controller.h"

namespace pico_toolset {

// The words of the Bluetooth page, shared by every view (the pixel one on a DisplayDriver, the LVGL
// one) so they say the same thing: only the drawing differs.

// How a piece of text should be coloured: Normal = body text, Dim = secondary, Ok / Warn / Bad = state.
enum class BtUiTone : uint8_t { Normal, Dim, Ok, Warn, Bad };

struct BtUiText {
    char     text[48];
    BtUiTone tone;
};

[[nodiscard]] const char* bt_ui_kind_tag(BtUiKind kind);             // "KBD", "MOUSE", "K+M", "PAD", "AUDIO", "DEV"
[[nodiscard]] BtUiTone bt_ui_state_tone(BtUiState state);             // Connected / Streaming ok, Connecting warn
void bt_ui_display_name(const BtUiDevice& device, char* out, size_t cap); // the name, or the address tail

// What an item's button says; `shortened` gives the form for narrow controls.
[[nodiscard]] const char* bt_ui_action_label(BtUiAction action, BtUiScreen screen, bool shortened);

// The title for the current screen (`default_title` on the list; with the bonded count when not empty).
void bt_ui_page_title(const BluetoothPageController& page, const char* default_title, char* out, size_t cap);

// The status line (passkey, connecting, notice, scanning). `idle_hint` is used when nothing else is
// to be said (e.g. the keys of a keyboard-only host); empty text when there is nothing to show.
[[nodiscard]] BtUiText bt_ui_status(const BluetoothPageController& page, const char* idle_hint = "");

// The row text of a device: its right-hand column is the state (bonded) or the signal (found).
// Empty when there is nothing to say.
void bt_ui_row_state(const BtUiItem& item, const BtUiDevice& device, char* out, size_t cap);
[[nodiscard]] BtUiTone bt_ui_row_state_tone(const BtUiItem& item, const BtUiDevice& device);

// Lines of plain text the page shows instead of rows: the detail of a device, the forget question, or
// the "nothing here" messages of an empty list. Returns the number of lines written (0 on a list that
// has rows).
size_t bt_ui_info_lines(const BluetoothPageController& page, BtUiText* out, size_t max);

} // namespace pico_toolset
