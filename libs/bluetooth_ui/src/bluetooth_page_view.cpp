#include "pico_toolset/bluetooth_page_view.h"

#include <cstdio>
#include <cstring>

#include "pico_toolset/simple_font.h"
#include "pico_toolset/widget.h"

namespace pico_toolset {

namespace {

struct Palette {
    Color bg, text, dim, header, focus, button, button_text, ok, warn, bad;
    bool mono;
};

Palette make_palette(const BtUiTheme& t, bool mono) {
    if (mono) return {kColorBlack, kColorWhite, kColorWhite, kColorBlack, kColorWhite, kColorBlack, kColorWhite,
                      kColorWhite, kColorWhite, kColorWhite, true};
    return {t.background, t.text, t.dim, t.header, t.focus, t.button, t.button_text, t.ok, t.warn, t.bad, false};
}

// Draws at most max_chars characters (the last one becomes '~' when the text is cut).
void draw_text(DisplayDriver& d, int x, int y, const char* text, Color fg, Color bg, int scale, int max_chars) {
    if (max_chars <= 0) return;
    char buf[48];
    const int len = static_cast<int>(std::strlen(text));
    int n = len < max_chars ? len : max_chars;
    if (n > static_cast<int>(sizeof(buf)) - 1) n = static_cast<int>(sizeof(buf)) - 1;
    std::memcpy(buf, text, static_cast<size_t>(n));
    buf[n] = '\0';
    if (len > n && n > 0) buf[n - 1] = '~';
    TextWidget w(x, y, buf, fg, bg, kGlyphFont5x8.glyphs, 8, static_cast<uint8_t>(scale));
    w.draw(d);
}

int text_px(int chars, int scale) { return chars > 0 ? chars * 6 * scale - scale : 0; }

void fill(DisplayDriver& d, const BtUiRect& r, Color c) {
    if (r.w > 0 && r.h > 0) d.fill_rect(r.x, r.y, r.x + r.w - 1, r.y + r.h - 1, c);
}

void outline(DisplayDriver& d, const BtUiRect& r, Color c) {
    if (r.w < 2 || r.h < 2) return;
    d.fill_rect(r.x, r.y, r.x + r.w - 1, r.y, c);
    d.fill_rect(r.x, r.y + r.h - 1, r.x + r.w - 1, r.y + r.h - 1, c);
    d.fill_rect(r.x, r.y, r.x, r.y + r.h - 1, c);
    d.fill_rect(r.x + r.w - 1, r.y, r.x + r.w - 1, r.y + r.h - 1, c);
}

const char* kind_tag(BtUiKind k) {
    switch (k) {
    case BtUiKind::Keyboard: return "KBD";
    case BtUiKind::Mouse: return "MOUSE";
    case BtUiKind::KeyboardMouse: return "K+M";
    case BtUiKind::Gamepad: return "PAD";
    case BtUiKind::Headset: return "AUDIO";
    default: return "DEV";
    }
}

void display_name(const BtUiDevice& dev, char* out, size_t cap) {
    if (dev.name[0]) std::snprintf(out, cap, "%s", dev.name);
    else std::snprintf(out, cap, "%02X:%02X:%02X", dev.addr[3], dev.addr[4], dev.addr[5]);
}

Color state_color(const Palette& p, BtUiState s) {
    switch (s) {
    case BtUiState::Connected:
    case BtUiState::Streaming: return p.ok;
    case BtUiState::Connecting: return p.warn;
    default: return p.dim;
    }
}

const char* action_label(BtUiAction a, BtUiScreen screen, bool shortened) {
    switch (a) {
    case BtUiAction::StartScan: return screen == BtUiScreen::Scan ? "SCAN" : (shortened ? "PAIR" : "PAIR NEW");
    case BtUiAction::StopScan: return "STOP";
    case BtUiAction::Close: return "CLOSE";
    case BtUiAction::Back: return "BACK";
    case BtUiAction::Connect: return shortened ? "CONN" : "CONNECT";
    case BtUiAction::Disconnect: return shortened ? "DISC" : "DISCONNECT";
    case BtUiAction::Forget: return "FORGET";
    case BtUiAction::ConfirmForget: return "YES";
    case BtUiAction::Cancel: return "NO";
    default: return "";
    }
}

} // namespace

void BluetoothPageView::recompute() {
    bt_ui_compute_layout(page_, BtUiLayoutConfig{cfg_.area, cfg_.input_caps, 28}, scroll_, layout_);
    scroll_ = layout_.first_row;
}

int BluetoothPageView::hit_test(int x, int y) {
    recompute();
    return bt_ui_hit_test(layout_, page_.item_count(), x, y);
}

void BluetoothPageView::draw(DisplayDriver& d) {
    recompute();
    const BtUiLayout& L = layout_;
    const Palette p = make_palette(cfg_.theme, d.is_monochrome());
    const int s = L.scale;
    const int cell_h = 8 * s;
    const int pad = std::max(2, s * 2);
    const int char_w = 6 * s;
    fill(d, cfg_.area, p.bg);

    // ---- header ----
    char buf[64], name[32];
    fill(d, L.header, p.header);
    const BtUiScreen screen = page_.screen();
    const char* title = cfg_.title;
    if (screen == BtUiScreen::Scan) title = "Pair new";
    else if (screen == BtUiScreen::Detail) { display_name(page_.selected(), name, sizeof(name)); title = name; }
    else if (screen == BtUiScreen::ConfirmForget) title = "Forget?";
    else if (page_.bonded_count() > 0) { std::snprintf(buf, sizeof(buf), "%s (%u)", cfg_.title, static_cast<unsigned>(page_.bonded_count())); title = buf; }
    const int header_y = L.header.y + (L.header.h - cell_h) / 2;
    draw_text(d, L.header.x + pad, header_y, title, p.text, p.header, s, (L.header.w - 2 * pad) / char_w);
    if (p.mono) d.fill_rect(L.header.x, L.header.y + L.header.h - 1, L.header.x + L.header.w - 1, L.header.y + L.header.h - 1, p.text);

    // ---- status (a separate line, or the header's right part on small displays) ----
    char status[48] = "";
    Color status_color = p.dim;
    uint32_t code = 0;
    if (page_.passkey(&code)) { std::snprintf(status, sizeof(status), "Type %06lu on the device", static_cast<unsigned long>(code)); status_color = p.warn; }
    else if (page_.connecting()) { std::snprintf(status, sizeof(status), "Connecting..."); status_color = p.warn; }
    else if (page_.notice() == BtUiNotice::Paired) { std::snprintf(status, sizeof(status), "Paired"); status_color = p.ok; }
    else if (page_.notice() == BtUiNotice::PairFailed) { std::snprintf(status, sizeof(status), "Failed (0x%02X)", page_.error()); status_color = p.bad; }
    else if (page_.notice() == BtUiNotice::Forgotten) { std::snprintf(status, sizeof(status), "Forgotten"); status_color = p.ok; }
    else if (page_.scanning()) std::snprintf(status, sizeof(status), "Scanning... %u found", static_cast<unsigned>(page_.found_count()));
    else if (screen == BtUiScreen::Scan) std::snprintf(status, sizeof(status), "Scan ended");
    else if (L.show_focus && !(cfg_.input_caps & bt_ui_input::Direct)) std::snprintf(status, sizeof(status), "Up/Down  Enter  Esc");
    if (status[0]) {
        if (L.status_in_header) {
            const int w = static_cast<int>(std::strlen(status)) * char_w;
            const int title_w = static_cast<int>(std::strlen(title)) * char_w + 2 * pad;
            if (L.header.w - title_w >= w) // otherwise the title keeps the line
                draw_text(d, L.header.x + L.header.w - pad - text_px(static_cast<int>(std::strlen(status)), s), header_y, status, status_color, p.header, s, 47);
        } else {
            draw_text(d, L.status.x + pad, L.status.y + (L.status.h - cell_h) / 2, status, status_color, p.bg, s, (L.status.w - 2 * pad) / char_w);
        }
    }

    // ---- list / info ----
    const int info_h = cell_h + pad;
    auto info = [&](size_t line, const char* text, Color c) {
        if (line >= L.info_lines) return;
        draw_text(d, L.list.x + pad, L.list.y + static_cast<int>(line) * info_h + pad / 2, text, c, p.bg, s, (L.list.w - 2 * pad) / char_w);
    };

    if (screen == BtUiScreen::Detail || screen == BtUiScreen::ConfirmForget) {
        const BtUiDevice& dev = page_.selected();
        if (screen == BtUiScreen::Detail) {
            std::snprintf(buf, sizeof(buf), "Kind: %s", bt_ui_kind_name(dev.kind));
            info(0, buf, p.text);
            std::snprintf(buf, sizeof(buf), "State: %s", bt_ui_state_name(dev.state));
            info(1, buf, state_color(p, dev.state));
            std::snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X", dev.addr[0], dev.addr[1], dev.addr[2], dev.addr[3], dev.addr[4], dev.addr[5]);
            info(2, buf, p.dim);
        } else {
            display_name(dev, name, sizeof(name));
            std::snprintf(buf, sizeof(buf), "Forget %s?", name);
            info(0, buf, p.text);
            info(1, "Pair it again to use it.", p.dim);
        }
    } else {
        bool any_row = false;
        for (size_t i = 0; i < page_.item_count(); ++i) {
            if (!bt_ui_item_is_row(page_.item(i).action)) continue;
            any_row = true;
            if (!L.item_visible[i]) continue;
            const BtUiRect& r = L.item_rect[i];
            const BtUiItem it = page_.item(i);
            const BtUiDevice& dev = it.action == BtUiAction::OpenDevice ? page_.bonded(static_cast<size_t>(it.index)) : page_.found(static_cast<size_t>(it.index));
            const bool focused = L.show_focus && i == page_.focus();
            const Color row_bg = focused ? p.focus : p.bg;
            const Color row_fg = focused && p.mono ? p.bg : p.text;
            fill(d, r, row_bg);
            const int dot = cell_h - 2 * s;
            const int dot_x = r.x + pad, dot_y = r.y + (r.h - dot) / 2;
            if (it.action == BtUiAction::OpenDevice) {
                const Color dc = state_color(p, dev.state);
                if (p.mono && dev.state == BtUiState::Offline) outline(d, {dot_x, dot_y, dot, dot}, p.mono && focused ? p.bg : p.text);
                else d.fill_rect(dot_x, dot_y, dot_x + dot - 1, dot_y + dot - 1, p.mono && focused ? p.bg : dc);
            }
            int cols = L.text_cols;
            int tx = dot_x + dot + pad;
            display_name(dev, name, sizeof(name));
            char state[16] = "";
            if (it.action == BtUiAction::OpenDevice && cols >= 26) std::snprintf(state, sizeof(state), "%s", bt_ui_state_name(dev.state));
            else if (it.action == BtUiAction::PickFound && dev.rssi != 0 && cols >= 26) std::snprintf(state, sizeof(state), "%d dBm", dev.rssi);
            const int state_cols = static_cast<int>(std::strlen(state));
            int name_cols = cols - (state_cols ? state_cols + 1 : 0);
            const int ty = r.y + (r.h - cell_h) / 2;
            if (cols >= 16) {
                const char* tag = kind_tag(dev.kind);
                draw_text(d, tx, ty, tag, p.mono ? row_fg : (focused ? p.text : p.dim), row_bg, s, 5);
                tx += 6 * char_w;
                name_cols -= 6;
            }
            draw_text(d, tx, ty, name, row_fg, row_bg, s, name_cols);
            if (state_cols) draw_text(d, r.x + r.w - pad - text_px(state_cols, s), ty, state, it.action == BtUiAction::OpenDevice ? (focused && p.mono ? p.bg : state_color(p, dev.state)) : row_fg, row_bg, s, state_cols);
        }
        if (!any_row) {
            if (screen == BtUiScreen::Scan) {
                info(0, page_.scanning() ? "Looking for devices..." : "Nothing found.", p.text);
                info(1, "Put the device in pairing mode.", p.dim);
            } else {
                info(0, "No paired devices.", p.dim);
            }
        }
        if (L.more_above) draw_text(d, L.list.x + L.list.w - char_w - pad / 2, L.list.y, "^", p.dim, p.bg, s, 1);
        if (L.more_below) draw_text(d, L.list.x + L.list.w - char_w - pad / 2, L.list.y + L.list.h - cell_h, "v", p.dim, p.bg, s, 1);
    }

    // ---- buttons ----
    for (size_t i = 0; i < page_.item_count(); ++i) {
        if (bt_ui_item_is_row(page_.item(i).action)) continue;
        const BtUiRect& r = L.item_rect[i];
        const bool focused = L.show_focus && i == page_.focus();
        const Color bg = focused ? (p.mono ? p.text : p.focus) : p.button;
        const Color fg = focused && p.mono ? p.bg : p.button_text;
        const BtUiRect inner{r.x + 1, r.y + 1, r.w - 2, r.h - 2};
        fill(d, inner, bg);
        if (p.mono) outline(d, r, p.text);
        const char* label = action_label(page_.item(i).action, screen, false);
        int ls = s;
        int chars = static_cast<int>(std::strlen(label));
        if (text_px(chars, ls) > inner.w - 2) label = action_label(page_.item(i).action, screen, true), chars = static_cast<int>(std::strlen(label));
        if (text_px(chars, ls) > inner.w - 2 && ls > 1) ls = 1;
        const int w = text_px(chars, ls);
        draw_text(d, inner.x + (inner.w - w) / 2, inner.y + (inner.h - 8 * ls) / 2, label, fg, bg, ls, (inner.w - 2) / (6 * ls));
    }
}

} // namespace pico_toolset
