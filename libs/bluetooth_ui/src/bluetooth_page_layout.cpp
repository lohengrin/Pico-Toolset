#include "pico_toolset/bluetooth_page_layout.h"

#include <algorithm>

namespace pico_toolset {

bool bt_ui_item_is_row(BtUiAction action) {
    return action == BtUiAction::OpenDevice || action == BtUiAction::PickFound;
}

namespace {

int pick_scale(const BtUiRect& a) {
    int s = a.w < 200 ? 1 : a.w < 360 ? 2 : a.w < 560 ? 3 : 4;
    // Keep at least ~6 text lines: header, 3 rows (or info lines), status, buttons.
    while (s > 1 && a.h < 6 * (8 * s + 4)) --s;
    return s;
}

} // namespace

void bt_ui_compute_layout(const BluetoothPageController& page, const BtUiLayoutConfig& cfg, size_t scroll,
                          BtUiLayout& out) {
    out = BtUiLayout{};
    const BtUiRect& a = cfg.area;
    out.scale = pick_scale(a);
    out.text_scale = std::max(1, (out.scale + 1) / 2);
    const int cell_h = 8 * out.text_scale;
    const int pad = std::max(2, out.text_scale * 2);
    const int title_pad = std::max(2, out.scale * 2);
    int row_h = cell_h + 2 * pad;
    if (cfg.input_caps & bt_ui_input::Direct) row_h = std::max(row_h, std::min(cfg.min_target_px, a.h / 8));
    out.row_h = row_h;
    out.show_focus = (cfg.input_caps & bt_ui_input::Navigate) != 0;

    out.header = {a.x, a.y, a.w, 8 * out.scale + 2 * title_pad};
    out.footer = {a.x, a.y + a.h - row_h, a.w, row_h};
    out.status_in_header = a.h < 90;
    const int status_h = out.status_in_header ? 0 : cell_h + pad;
    out.status = out.status_in_header ? out.header : BtUiRect{a.x, out.footer.y - status_h, a.w, status_h};
    const int list_y = out.header.y + out.header.h;
    out.list = {a.x, list_y, a.w, std::max(0, out.footer.y - status_h - list_y)};
    out.text_cols = std::max(0, (a.w - 2 * pad - cell_h - pad) / (6 * out.text_scale));
    out.visible_rows = static_cast<size_t>(out.list.h / row_h);
    out.info_lines = static_cast<size_t>(out.list.h / (cell_h + pad));

    size_t rows = 0, buttons = 0;
    for (size_t i = 0; i < page.item_count(); ++i) {
        if (bt_ui_item_is_row(page.item(i).action)) ++rows;
        else ++buttons;
    }

    size_t first = scroll;
    if (rows <= out.visible_rows || out.visible_rows == 0) {
        first = 0;
    } else {
        size_t focus_row = rows; // rows before the focused item, when the focus is on a row
        size_t k = 0;
        for (size_t i = 0; i < page.item_count(); ++i) {
            if (!bt_ui_item_is_row(page.item(i).action)) continue;
            if (i == page.focus()) focus_row = k;
            ++k;
        }
        if (focus_row < rows) {
            if (focus_row < first) first = focus_row;
            if (focus_row >= first + out.visible_rows) first = focus_row + 1 - out.visible_rows;
        }
        first = std::min(first, rows - out.visible_rows);
    }
    out.first_row = first;
    out.more_above = first > 0;
    out.more_below = rows > first + out.visible_rows;

    size_t row_index = 0, button_index = 0;
    for (size_t i = 0; i < page.item_count(); ++i) {
        if (bt_ui_item_is_row(page.item(i).action)) {
            const bool vis = row_index >= first && row_index < first + out.visible_rows;
            out.item_visible[i] = vis;
            out.item_rect[i] = {out.list.x, out.list.y + static_cast<int>(row_index - first) * row_h, out.list.w, row_h};
            ++row_index;
        } else {
            const int bw = buttons ? out.footer.w / static_cast<int>(buttons) : out.footer.w;
            const int bx = out.footer.x + static_cast<int>(button_index) * bw;
            const int w = button_index + 1 == buttons ? out.footer.x + out.footer.w - bx : bw; // absorb the rounding
            out.item_visible[i] = true;
            out.item_rect[i] = {bx, out.footer.y, w, out.footer.h};
            ++button_index;
        }
    }
}

int bt_ui_hit_test(const BtUiLayout& layout, size_t item_count, int x, int y) {
    for (size_t i = 0; i < item_count && i < BluetoothPageController::kMaxItems; ++i)
        if (layout.item_visible[i] && layout.item_rect[i].contains(x, y)) return static_cast<int>(i);
    return -1;
}

} // namespace pico_toolset
