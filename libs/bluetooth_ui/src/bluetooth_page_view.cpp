#include "pico_toolset/bluetooth_page_view.h"

#include <cstdio>
#include <cstring>

#include "pico_toolset/bluetooth_page_text.h"
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

Color tone_color(const Palette& p, BtUiTone t) {
    switch (t) {
    case BtUiTone::Dim: return p.dim;
    case BtUiTone::Ok: return p.ok;
    case BtUiTone::Warn: return p.warn;
    case BtUiTone::Bad: return p.bad;
    default: return p.text;
    }
}

Color state_color(const Palette& p, BtUiState s) { return tone_color(p, bt_ui_state_tone(s)); }

} // namespace

void BluetoothPageView::recompute() {
    bt_ui_compute_layout(page_, BtUiLayoutConfig{cfg_.area, cfg_.input_caps, 28}, scroll_, layout_);
    scroll_ = layout_.first_row;
}

int BluetoothPageView::hit_test(int x, int y) {
    recompute();
    return bt_ui_hit_test(layout_, page_.item_count(), x, y);
}

namespace {

// Passes through only what lands inside `clip`.
class ClipDisplay final : public DisplayDriver {
public:
    ClipDisplay(DisplayDriver& target, const BtUiRect& clip) : t_(target), c_(clip) {}
    int width() const override { return t_.width(); }
    int height() const override { return t_.height(); }
    bool is_monochrome() const override { return t_.is_monochrome(); }
    void set_pixel(int x, int y, Color color) override {
        if (c_.contains(x, y)) t_.set_pixel(x, y, color);
    }
    void fill_rect(int x0, int y0, int x1, int y1, Color color) override {
        x0 = std::max(x0, c_.x);
        y0 = std::max(y0, c_.y);
        x1 = std::min(x1, c_.x + c_.w - 1);
        y1 = std::min(y1, c_.y + c_.h - 1);
        if (x0 <= x1 && y0 <= y1) t_.fill_rect(x0, y0, x1, y1, color);
    }

private:
    DisplayDriver& t_;
    BtUiRect c_;
};

} // namespace

void BluetoothPageView::draw(DisplayDriver& d, const BtUiRect* clip) {
    if (!clip) {
        draw_all(d);
        return;
    }
    ClipDisplay clipped(d, *clip);
    draw_all(clipped);
}

void BluetoothPageView::draw_all(DisplayDriver& d) {
    recompute();
    const BtUiLayout& L = layout_;
    const Palette p = make_palette(cfg_.theme, d.is_monochrome());
    const int ts = L.scale;       // the title; everything else is half that size
    const int s = L.text_scale;
    const int cell_h = 8 * s;
    const int pad = std::max(2, s * 2);
    const int char_w = 6 * s;
    const int title_pad = std::max(2, ts * 2);
    fill(d, cfg_.area, p.bg);

    // ---- header ----
    char buf[64], name[32];
    fill(d, L.header, p.header);
    const BtUiScreen screen = page_.screen();
    bt_ui_page_title(page_, cfg_.title, buf, sizeof(buf));
    const char* title = buf;
    const int header_y = L.header.y + (L.header.h - 8 * ts) / 2;
    draw_text(d, L.header.x + title_pad, header_y, title, p.text, p.header, ts, (L.header.w - 2 * title_pad) / (6 * ts));
    if (p.mono) d.fill_rect(L.header.x, L.header.y + L.header.h - 1, L.header.x + L.header.w - 1, L.header.y + L.header.h - 1, p.text);

    // ---- status (a separate line, or the header's right part on small displays) ----
    const bool key_hint = L.show_focus && !(cfg_.input_caps & bt_ui_input::Direct);
    const BtUiText st = bt_ui_status(page_, key_hint ? "Up/Down  Enter  Esc" : "");
    const char* status = st.text;
    const Color status_color = tone_color(p, st.tone);
    if (status[0]) {
        if (L.status_in_header) {
            const int w = static_cast<int>(std::strlen(status)) * char_w;
            const int title_w = static_cast<int>(std::strlen(title)) * 6 * ts + 2 * title_pad;
            if (L.header.w - title_w >= w) // otherwise the title keeps the line
                draw_text(d, L.header.x + L.header.w - pad - text_px(static_cast<int>(std::strlen(status)), s),
                          L.header.y + (L.header.h - cell_h) / 2, status, status_color, p.header, s, 47);
        } else {
            draw_text(d, L.status.x + pad, L.status.y + (L.status.h - cell_h) / 2, status, status_color, p.bg, s, (L.status.w - 2 * pad) / char_w);
        }
    }

    // ---- list / info ----
    const int info_h = cell_h + pad;
    BtUiText lines[3];
    const size_t line_count = bt_ui_info_lines(page_, lines, 3);
    for (size_t i = 0; i < line_count && i < L.info_lines; ++i)
        draw_text(d, L.list.x + pad, L.list.y + static_cast<int>(i) * info_h + pad / 2, lines[i].text,
                  tone_color(p, lines[i].tone), p.bg, s, (L.list.w - 2 * pad) / char_w);

    if (screen != BtUiScreen::Detail && screen != BtUiScreen::ConfirmForget) {
        for (size_t i = 0; i < page_.item_count(); ++i) {
            if (!bt_ui_item_is_row(page_.item(i).action)) continue;
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
            bt_ui_display_name(dev, name, sizeof(name));
            char state[16] = "";
            if (cols >= 26) bt_ui_row_state(it, dev, state, sizeof(state));
            const int state_cols = static_cast<int>(std::strlen(state));
            int name_cols = cols - (state_cols ? state_cols + 1 : 0);
            const int ty = r.y + (r.h - cell_h) / 2;
            if (cols >= 16) {
                const char* tag = bt_ui_kind_tag(dev.kind);
                draw_text(d, tx, ty, tag, p.mono ? row_fg : (focused ? p.text : p.dim), row_bg, s, 5);
                tx += 6 * char_w;
                name_cols -= 6;
            }
            draw_text(d, tx, ty, name, row_fg, row_bg, s, name_cols);
            if (state_cols) draw_text(d, r.x + r.w - pad - text_px(state_cols, s), ty, state, it.action == BtUiAction::OpenDevice ? (focused && p.mono ? p.bg : state_color(p, dev.state)) : row_fg, row_bg, s, state_cols);
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
        const char* label = bt_ui_action_label(page_.item(i).action, screen, false);
        int ls = s;
        int chars = static_cast<int>(std::strlen(label));
        if (text_px(chars, ls) > inner.w - 2) label = bt_ui_action_label(page_.item(i).action, screen, true), chars = static_cast<int>(std::strlen(label));
        if (text_px(chars, ls) > inner.w - 2 && ls > 1) ls = 1;
        const int w = text_px(chars, ls);
        draw_text(d, inner.x + (inner.w - w) / 2, inner.y + (inner.h - 8 * ls) / 2, label, fg, bg, ls, (inner.w - 2) / (6 * ls));
    }
}

} // namespace pico_toolset
