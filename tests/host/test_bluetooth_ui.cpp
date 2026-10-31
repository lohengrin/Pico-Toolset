// Host tests for the common Bluetooth page: device merging and the page state machine.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <vector>

#include "pico_toolset/bluetooth_page_input.h"
#include "pico_toolset/bluetooth_page_view.h"

using namespace pico_toolset;

namespace {

int g_failures = 0;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
            ++g_failures;                                                                 \
        }                                                                                 \
    } while (0)

BtUiDevice make(BtUiSource src, uint8_t id, const char* name, BtUiKind kind, BtUiState state = BtUiState::Offline) {
    BtUiDevice d{};
    d.addr[5] = id;
    std::strncpy(d.name, name, sizeof(d.name) - 1);
    d.source = src;
    d.kind = kind;
    d.state = state;
    return d;
}

class FakeBackend final : public BtUiBackend {
public:
    FakeBackend(BtUiSource s, bool can_disc) : src(s), can_disc(can_disc) {}
    BtUiSource src;
    bool can_disc;
    std::vector<BtUiDevice> paired, seen;
    bool scan = false, busy = false;
    uint8_t error = 0;
    uint32_t counter = 0;
    bool pair_succeeds = true;
    int connects = 0, forgets = 0, disconnects = 0;

    BtUiSource source() const override { return src; }
    uint32_t change_counter() const override { return counter; }
    size_t bonded(BtUiDevice* out, size_t max) override {
        size_t n = 0;
        for (; n < paired.size() && n < max; ++n) out[n] = paired[n];
        return n;
    }
    size_t found(BtUiDevice* out, size_t max) override {
        size_t n = 0;
        for (; n < seen.size() && n < max; ++n) out[n] = seen[n];
        return n;
    }
    bool scanning() const override { return scan; }
    void start_scan() override { scan = true; seen.clear(); ++counter; }
    void stop_scan() override { scan = false; ++counter; }
    bool connect(const BtUiDevice& d) override {
        ++connects;
        scan = false;
        error = 0;
        for (auto& p : paired)
            if (bt_ui_same_device(p, d)) { p.state = BtUiState::Connected; ++counter; return true; }
        if (pair_succeeds) {
            BtUiDevice p = d;
            p.state = BtUiState::Connected;
            paired.push_back(p);
        } else {
            error = 0x05;
        }
        ++counter;
        return true;
    }
    bool can_disconnect(const BtUiDevice&) const override { return can_disc; }
    void disconnect(const BtUiDevice& d) override {
        ++disconnects;
        for (auto& p : paired) if (bt_ui_same_device(p, d)) p.state = BtUiState::Offline;
        ++counter;
    }
    bool forget(const BtUiDevice& d) override {
        ++forgets;
        for (size_t i = 0; i < paired.size(); ++i)
            if (bt_ui_same_device(paired[i], d)) { paired.erase(paired.begin() + i); ++counter; return true; }
        return false;
    }
    bool connecting() const override { return busy; }
    bool passkey(uint32_t* c) const override {
        if (!passkey_value) return false;
        if (c) *c = passkey_value;
        return true;
    }
    uint8_t last_error() const override { return error; }
    uint32_t passkey_value = 0;
};

void test_devices_merge() {
    FakeBackend hid(BtUiSource::Hid, false), audio(BtUiSource::Audio, true);
    BluetoothDevices devs;
    CHECK(devs.add_backend(hid));
    CHECK(devs.add_backend(audio));
    hid.paired = {make(BtUiSource::Hid, 1, "Keys", BtUiKind::Keyboard, BtUiState::Connected),
                  make(BtUiSource::Hid, 2, "Pad", BtUiKind::Gamepad)};
    audio.paired = {make(BtUiSource::Audio, 9, "Headset", BtUiKind::Headset)};

    BtUiDevice out[8];
    CHECK(devs.bonded(out, 8) == 3);
    CHECK(out[2].source == BtUiSource::Audio);
    CHECK(devs.bonded(out, 2) == 2);

    devs.start_scan();
    CHECK(devs.scanning() && hid.scan && audio.scan);
    hid.seen = {make(BtUiSource::Hid, 1, "Keys", BtUiKind::Keyboard), // already bonded: hidden
                make(BtUiSource::Hid, 5, "Mouse", BtUiKind::Mouse)};
    audio.seen = {make(BtUiSource::Audio, 5, "Mouse again", BtUiKind::Headset), // same address as the HID one
                  make(BtUiSource::Audio, 7, "Speaker", BtUiKind::Headset)};
    BtUiDevice f[16];
    const size_t n = devs.found(f, 16);
    CHECK(n == 2);
    CHECK(f[0].addr[5] == 5 && f[1].addr[5] == 7);

    const uint32_t before = devs.change_counter();
    audio.counter += 3;
    CHECK(devs.change_counter() == before + 3);

    CHECK(devs.connect(f[1]));
    CHECK(!audio.scan && !hid.scan); // connecting ends scans on all backends
    CHECK(audio.connects == 1 && hid.connects == 0);
    CHECK(!devs.can_disconnect(hid.paired[0]));
    CHECK(devs.can_disconnect(audio.paired[0]));
}

struct Rig {
    FakeBackend hid{BtUiSource::Hid, false};
    FakeBackend audio{BtUiSource::Audio, true};
    BluetoothDevices devs;
    BluetoothPageController page{devs};
    Rig() {
        devs.add_backend(hid);
        devs.add_backend(audio);
        hid.paired = {make(BtUiSource::Hid, 1, "Keys", BtUiKind::Keyboard, BtUiState::Connected)};
        audio.paired = {make(BtUiSource::Audio, 9, "Headset", BtUiKind::Headset)};
        page.open();
        page.update();
    }
    bool has(BtUiAction a, int idx = -1) const {
        for (size_t i = 0; i < page.item_count(); ++i)
            if (page.item(i).action == a && page.item(i).index == idx) return true;
        return false;
    }
    void focus_on(BtUiAction a, int idx = -1) {
        for (size_t i = 0; i < page.item_count(); ++i)
            if (page.item(i).action == a && page.item(i).index == idx) page.set_focus(i);
    }
    void select(BtUiAction a, int idx = -1) { focus_on(a, idx); page.handle(BtUiInput::Select); }
};

void test_list_and_navigation() {
    Rig r;
    CHECK(r.page.screen() == BtUiScreen::List);
    CHECK(r.page.bonded_count() == 2);
    CHECK(r.page.item_count() == 4); // two devices + pair new + close
    CHECK(r.page.focus() == 0);
    r.page.handle(BtUiInput::Prev);
    CHECK(r.page.focus() == 0); // clamped
    for (int i = 0; i < 10; ++i) r.page.handle(BtUiInput::Next);
    CHECK(r.page.focus() == 3);
    CHECK(r.page.item(3).action == BtUiAction::Close);
    CHECK(r.page.update()); // input made it dirty
    CHECK(!r.page.update());
    r.page.handle(BtUiInput::Select);
    CHECK(r.page.closed());
    r.page.open();
    CHECK(!r.page.closed());
    r.page.handle(BtUiInput::Back);
    CHECK(r.page.closed()); // Back on the list leaves the page
}

void test_detail_and_forget() {
    Rig r;
    r.page.activate(0); // the keyboard: HID cannot disconnect
    CHECK(r.page.screen() == BtUiScreen::Detail);
    CHECK(std::strcmp(r.page.selected().name, "Keys") == 0);
    CHECK(!r.has(BtUiAction::Connect) && !r.has(BtUiAction::Disconnect));
    CHECK(r.has(BtUiAction::Forget) && r.has(BtUiAction::Back));
    r.page.handle(BtUiInput::Back);
    CHECK(r.page.screen() == BtUiScreen::List);

    r.page.activate(1); // the headset: offline, can connect
    CHECK(r.has(BtUiAction::Connect));
    r.select(BtUiAction::Connect);
    CHECK(r.audio.connects == 1);
    r.page.update();
    CHECK(r.page.selected().state == BtUiState::Connected);
    CHECK(r.has(BtUiAction::Disconnect) && !r.has(BtUiAction::Connect));
    r.select(BtUiAction::Disconnect);
    CHECK(r.audio.disconnects == 1);
    r.page.update();
    CHECK(r.has(BtUiAction::Connect));

    r.select(BtUiAction::Forget);
    CHECK(r.page.screen() == BtUiScreen::ConfirmForget);
    CHECK(r.page.item(r.page.focus()).action == BtUiAction::Cancel); // safe default
    r.page.handle(BtUiInput::Back);
    CHECK(r.page.screen() == BtUiScreen::Detail);
    CHECK(r.audio.forgets == 0);
    r.select(BtUiAction::Forget);
    r.select(BtUiAction::ConfirmForget);
    CHECK(r.audio.forgets == 1);
    CHECK(r.page.screen() == BtUiScreen::List);
    CHECK(r.page.notice() == BtUiNotice::Forgotten);
    CHECK(r.page.bonded_count() == 1);
}

void test_detail_follows_external_forget() {
    Rig r;
    r.page.activate(0);
    CHECK(r.page.screen() == BtUiScreen::Detail);
    r.hid.paired.clear(); // forgotten from outside the page
    ++r.hid.counter;
    CHECK(r.page.update());
    CHECK(r.page.screen() == BtUiScreen::List);
    CHECK(r.page.notice() == BtUiNotice::Forgotten);
}

void test_pairing_success() {
    Rig r;
    r.select(BtUiAction::StartScan);
    CHECK(r.page.screen() == BtUiScreen::Scan);
    CHECK(r.hid.scan && r.audio.scan);
    CHECK(r.page.scanning());
    CHECK(r.has(BtUiAction::StopScan) && !r.has(BtUiAction::StartScan));
    r.hid.seen = {make(BtUiSource::Hid, 5, "Mouse", BtUiKind::Mouse)};
    r.audio.seen = {make(BtUiSource::Audio, 7, "Speaker", BtUiKind::Headset)};
    ++r.hid.counter;
    CHECK(r.page.update());
    CHECK(r.page.found_count() == 2);
    CHECK(r.has(BtUiAction::PickFound, 0) && r.has(BtUiAction::PickFound, 1));

    r.select(BtUiAction::PickFound, 0);
    CHECK(r.hid.connects == 1);
    CHECK(r.page.screen() == BtUiScreen::List); // paired: back to the list
    CHECK(r.page.notice() == BtUiNotice::Paired);
    CHECK(r.page.bonded_count() == 3);
    CHECK(!r.hid.scan && !r.audio.scan);
}

void test_pairing_failure_and_passkey() {
    Rig r;
    r.hid.pair_succeeds = false;
    r.select(BtUiAction::StartScan);
    r.hid.seen = {make(BtUiSource::Hid, 5, "Keys2", BtUiKind::Keyboard)};
    ++r.hid.counter;
    r.page.update();
    r.select(BtUiAction::PickFound, 0);
    CHECK(r.page.screen() == BtUiScreen::Scan);
    CHECK(r.page.notice() == BtUiNotice::PairFailed);
    CHECK(r.page.error() == 0x05);
    CHECK(r.page.bonded_count() == 2);
    r.select(BtUiAction::StartScan); // retry clears the notice and scans again
    CHECK(r.page.notice() == BtUiNotice::None);
    CHECK(r.hid.scan);
}

void test_passkey_shown() {
    Rig r;
    r.select(BtUiAction::StartScan);
    r.hid.seen = {make(BtUiSource::Hid, 5, "Keys2", BtUiKind::Keyboard)};
    ++r.hid.counter;
    r.page.update();
    r.hid.busy = true;
    r.hid.passkey_value = 482913;
    ++r.hid.counter;
    CHECK(r.page.update());
    uint32_t code = 0;
    CHECK(r.page.passkey(&code) && code == 482913);
    CHECK(r.page.connecting());
}

void test_back_stops_scan() {
    Rig r;
    r.select(BtUiAction::StartScan);
    CHECK(r.hid.scan);
    r.page.handle(BtUiInput::Back);
    CHECK(r.page.screen() == BtUiScreen::List);
    CHECK(!r.hid.scan && !r.audio.scan);
    r.select(BtUiAction::StartScan);
    r.select(BtUiAction::Back);
    CHECK(!r.hid.scan);
    r.select(BtUiAction::StartScan);
    r.page.activate(r.page.item_count() - 1); // Back
    CHECK(r.page.screen() == BtUiScreen::List);
    r.select(BtUiAction::StartScan);
    r.select(BtUiAction::StopScan);
    CHECK(!r.hid.scan && r.has(BtUiAction::StartScan));
}

void test_focus_follows_item() {
    Rig r;
    r.select(BtUiAction::StartScan);
    r.hid.seen = {make(BtUiSource::Hid, 5, "A", BtUiKind::Mouse)};
    ++r.hid.counter;
    r.page.update();
    r.focus_on(BtUiAction::Back);
    r.hid.seen.push_back(make(BtUiSource::Hid, 6, "B", BtUiKind::Mouse)); // a new row appears above
    ++r.hid.counter;
    r.page.update();
    CHECK(r.page.item(r.page.focus()).action == BtUiAction::Back); // focus stays on Back
}

class FakeDisplay final : public DisplayDriver {
public:
    FakeDisplay(int w, int h, bool mono) : w_(w), h_(h), mono_(mono), px_(static_cast<size_t>(w * h), 0) {}
    int width() const override { return w_; }
    int height() const override { return h_; }
    bool is_monochrome() const override { return mono_; }
    void set_pixel(int x, int y, Color c) override {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) { ++oob; return; }
        px_[static_cast<size_t>(y * w_ + x)] = c.rgb565;
    }
    uint16_t at(int x, int y) const { return px_[static_cast<size_t>(y * w_ + x)]; }
    int oob = 0;
    // 0 = unset, '#' lit, '.' other colour (as a 1-bit view for eyeballing).
    void dump(const BtUiRect& r, int step) const {
        for (int y = r.y; y < r.y + r.h; y += step) {
            for (int x = r.x; x < r.x + r.w; x += step) std::putchar(at(x, y) == 0 ? ' ' : at(x, y) == 0xFFFF ? '#' : '+');
            std::putchar('\n');
        }
    }
private:
    int w_, h_;
    bool mono_;
    std::vector<uint16_t> px_;
};

bool inside(const BtUiRect& r, const BtUiRect& area) {
    return r.x >= area.x && r.y >= area.y && r.x + r.w <= area.x + area.w && r.y + r.h <= area.y + area.h;
}
bool overlap(const BtUiRect& a, const BtUiRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

void check_layout(Rig& r, int w, int h, uint8_t caps) {
    BtUiLayoutConfig cfg;
    cfg.area = {0, 0, w, h};
    cfg.input_caps = caps;
    BtUiLayout L;
    bt_ui_compute_layout(r.page, cfg, 0, L);
    CHECK(L.scale >= 1 && L.scale <= 4);
    CHECK(L.text_scale >= 1 && L.text_scale <= L.scale);
    CHECK(L.row_h >= 8 * L.text_scale);
    if (caps & bt_ui_input::Direct) CHECK(L.row_h >= std::min(28, h / 8));
    CHECK(L.show_focus == ((caps & bt_ui_input::Navigate) != 0));
    CHECK(L.visible_rows >= 1);
    for (size_t i = 0; i < r.page.item_count(); ++i) {
        if (!L.item_visible[i]) continue;
        CHECK(inside(L.item_rect[i], cfg.area));
        CHECK(L.item_rect[i].h >= 8 * L.text_scale);
        for (size_t k = i + 1; k < r.page.item_count(); ++k)
            if (L.item_visible[k]) CHECK(!overlap(L.item_rect[i], L.item_rect[k]));
        const BtUiRect& q = L.item_rect[i];
        CHECK(bt_ui_hit_test(L, r.page.item_count(), q.x + q.w / 2, q.y + q.h / 2) == static_cast<int>(i));
    }
}

void test_layout_sizes() {
    const struct { int w, h; uint8_t caps; } sizes[] = {
        {128, 64, bt_ui_input::Buttons}, {128, 64, bt_ui_input::Keyboard}, {240, 240, bt_ui_input::Touch},
        {320, 240, bt_ui_input::Touch | bt_ui_input::Keyboard}, {480, 480, bt_ui_input::Touch},
        {480, 480, bt_ui_input::Pointer | bt_ui_input::Pad}, {480, 800, bt_ui_input::Keyboard}};
    for (const auto& z : sizes) {
        Rig r;
        check_layout(r, z.w, z.h, z.caps);          // list
        r.page.activate(0);
        check_layout(r, z.w, z.h, z.caps);          // detail
        r.page.handle(BtUiInput::Back);
        r.select(BtUiAction::StartScan);
        r.hid.seen = {make(BtUiSource::Hid, 5, "Mouse", BtUiKind::Mouse)};
        ++r.hid.counter;
        r.page.update();
        check_layout(r, z.w, z.h, z.caps);          // scan
    }
    Rig a, b;
    BtUiLayout small, big;
    bt_ui_compute_layout(a.page, {{0, 0, 128, 64}, bt_ui_input::Buttons, 28}, 0, small);
    bt_ui_compute_layout(b.page, {{0, 0, 480, 480}, bt_ui_input::Touch, 28}, 0, big);
    CHECK(big.scale > small.scale);
    CHECK(big.text_cols >= 24 && small.text_cols >= 14);
}

void test_layout_scrolls_to_focus() {
    Rig r;
    r.select(BtUiAction::StartScan);
    for (int i = 0; i < 12; ++i)
        r.hid.seen.push_back(make(BtUiSource::Hid, static_cast<uint8_t>(20 + i), "Dev", BtUiKind::Gamepad));
    ++r.hid.counter;
    r.page.update();
    BtUiLayoutConfig cfg{{0, 0, 128, 64}, bt_ui_input::Buttons, 28};
    BtUiLayout L;
    bt_ui_compute_layout(r.page, cfg, 0, L);
    CHECK(L.visible_rows < 12 && L.more_below && !L.more_above);
    r.page.set_focus(11);   // the last found device
    size_t scroll = 0;
    for (int i = 0; i < 11; ++i) {
        bt_ui_compute_layout(r.page, cfg, scroll, L);
        scroll = L.first_row;
    }
    bt_ui_compute_layout(r.page, cfg, scroll, L);
    CHECK(L.item_visible[r.page.focus()]);
    CHECK(L.more_above && !L.more_below);
    r.page.set_focus(0);
    bt_ui_compute_layout(r.page, cfg, L.first_row, L);
    CHECK(L.first_row == 0 && L.item_visible[0]);
}

void render_checks(int dw, int dh, BtUiRect area, bool mono, uint8_t caps, bool dump) {
    Rig r;
    r.audio.paired[0].state = BtUiState::Streaming;
    r.page.update();
    FakeDisplay d(dw, dh, mono);
    BluetoothPageView view(r.page, {area, caps});
    view.draw(d);
    CHECK(d.oob == 0);
    int drawn = 0;
    for (int y = 0; y < dh; ++y)
        for (int x = 0; x < dw; ++x) {
            const bool in = x >= area.x && x < area.x + area.w && y >= area.y && y < area.y + area.h;
            if (!in) CHECK(d.at(x, y) == 0);
            if (in && d.at(x, y) == 0xFFFF) ++drawn;
        }
    CHECK(drawn > 50); // text was drawn
    if (dump) d.dump(area, mono ? 1 : 3);
    // every screen draws inside the area too
    r.page.activate(0);
    view.draw(d);
    r.select(BtUiAction::Forget);
    view.draw(d);
    r.page.handle(BtUiInput::Back);
    r.page.handle(BtUiInput::Back);
    r.select(BtUiAction::StartScan);
    r.hid.seen = {make(BtUiSource::Hid, 5, "A rather long device name here", BtUiKind::Keyboard)};
    r.hid.seen[0].rssi = -60;
    ++r.hid.counter;
    r.page.update();
    view.draw(d);
    if (dump) { std::puts("--- scan"); d.dump(area, mono ? 1 : 3); }
    CHECK(d.oob == 0);
    const int hit = view.hit_test(area.x + area.w / 2, area.y + area.h - 2);
    CHECK(hit >= 0 && r.page.item(static_cast<size_t>(hit)).action != BtUiAction::PickFound);
}

void test_view_render() {
    const bool dump = std::getenv("BT_UI_DUMP") != nullptr;
    render_checks(128, 64, {0, 0, 128, 64}, true, bt_ui_input::Buttons, dump);
    render_checks(240, 240, {0, 0, 240, 240}, false, bt_ui_input::Touch, dump);
    render_checks(400, 300, {40, 30, 320, 240}, false, bt_ui_input::Touch | bt_ui_input::Keyboard, false); // a pane
    render_checks(480, 480, {0, 0, 480, 480}, false, bt_ui_input::Pointer, false);
}

class FakeHid final : public HidSource {
public:
    bool keys[256]{};
    uint8_t modifiers = 0;
    uint8_t keyboards = 0;
    HidMouse mouse;
    int wheel = 0;
    GamepadState pad;
    bool is_key_down(uint8_t u) const override { return keys[u]; }
    bool is_modifier_down(uint8_t m) const override { return (modifiers & m) != 0; }
    HidMouse mouse_state() const override { return mouse; }
    void consume_mouse_delta(int&, int&) override {}
    int consume_mouse_wheel() override { int w = wheel; wheel = 0; return w; }
    GamepadState gamepad_state(size_t i) const override { return i == 0 ? pad : GamepadState{}; }
    uint8_t connected_keyboard_count() const override { return keyboards; }
};

struct InputRig {
    Rig rig;
    FakeHid hid;
    BluetoothPageView view{rig.page, {{0, 0, 240, 240}, bt_ui_input::Touch}};
    BluetoothPageInput input;
    explicit InputRig(BluetoothPageInputConfig cfg = {}) : input((cfg.hid = &hid, cfg)) {
        view.draw(dummy);
    }
    struct Null final : DisplayDriver {
        int width() const override { return 240; }
        int height() const override { return 240; }
        void set_pixel(int, int, Color) override {}
        void fill_rect(int, int, int, int, Color) override {}
    } dummy;
    void poll(const BtUiPointer* touch = nullptr, int button = -1) { input.poll(rig.page, view, touch, button); }
    BtUiRect rect_of(BtUiAction a, int idx = -1) {
        view.draw(dummy);
        for (size_t i = 0; i < rig.page.item_count(); ++i)
            if (rig.page.item(i).action == a && rig.page.item(i).index == idx) return view.layout().item_rect[i];
        return {};
    }
};

void test_input_keyboard() {
    InputRig r;
    CHECK(!(r.input.capabilities() & bt_ui_input::Keyboard));
    r.hid.keyboards = 1;
    r.poll();
    CHECK(r.input.capabilities() & bt_ui_input::Keyboard);
    r.hid.keys[0x51] = true;           // down
    r.poll();
    CHECK(r.rig.page.focus() == 1);
    for (int i = 0; i < 10; ++i) r.poll();   // held, not yet repeating
    CHECK(r.rig.page.focus() == 1);
    for (int i = 0; i < 30; ++i) r.poll();   // repeating
    CHECK(r.rig.page.focus() > 1);
    r.hid.keys[0x51] = false;
    r.poll();
    const size_t f = r.rig.page.focus();
    r.hid.keys[0x52] = true;           // up
    r.poll();
    r.hid.keys[0x52] = false;
    r.poll();
    CHECK(r.rig.page.focus() == f - 1);
    r.hid.keys[0x2B] = true;           // tab = next, shift+tab = previous
    r.poll();
    r.hid.keys[0x2B] = false;
    r.poll();
    CHECK(r.rig.page.focus() == f);
    r.hid.modifiers = 0x02;
    r.hid.keys[0x2B] = true;
    r.poll();
    r.hid.keys[0x2B] = false;
    r.hid.modifiers = 0;
    r.poll();
    CHECK(r.rig.page.focus() == f - 1);

    r.rig.page.set_focus(0);
    r.hid.keys[0x28] = true;           // enter on the first device
    r.poll();
    r.poll();                           // held: selects once
    CHECK(r.rig.page.screen() == BtUiScreen::Detail);
    r.hid.keys[0x28] = false;
    r.poll();
    r.hid.keys[0x29] = true;           // escape
    r.poll();
    CHECK(r.rig.page.screen() == BtUiScreen::List);
    r.hid.keys[0x29] = false;
    r.poll();
    r.hid.keyboards = 0;
    r.poll();
    CHECK(!(r.input.capabilities() & bt_ui_input::Keyboard));
}

void test_input_pad() {
    InputRig r;
    r.hid.pad.present = true;
    r.poll();
    CHECK(r.input.capabilities() & bt_ui_input::Pad);
    r.hid.pad.buttons = kBtDown;
    r.poll();
    r.hid.pad.buttons = 0;
    r.poll();
    CHECK(r.rig.page.focus() == 1);
    r.hid.pad.ly = 255;                // stick down
    r.poll();
    r.hid.pad.ly = 128;
    r.poll();
    CHECK(r.rig.page.focus() == 2);
    r.rig.page.set_focus(0);
    r.hid.pad.buttons = kBtA;
    r.poll();
    r.hid.pad.buttons = 0;
    r.poll();
    CHECK(r.rig.page.screen() == BtUiScreen::Detail);
    r.hid.pad.buttons = kBtB;
    r.poll();
    CHECK(r.rig.page.screen() == BtUiScreen::List);
}

void test_input_touch_tap() {
    BluetoothPageInputConfig cfg;
    cfg.has_touch = true;
    InputRig r(cfg);
    r.poll();
    CHECK(r.input.capabilities() & bt_ui_input::Touch);
    const BtUiRect dev = r.rect_of(BtUiAction::OpenDevice, 1);
    BtUiPointer p{true, dev.x + 10, dev.y + dev.h / 2};
    r.poll(&p);
    CHECK(r.rig.page.screen() == BtUiScreen::List);     // nothing until the finger lifts
    CHECK(r.rig.page.focus() == 1);                      // but the press focuses it
    p.down = false;
    r.poll(&p);
    CHECK(r.rig.page.screen() == BtUiScreen::Detail);
    CHECK(std::strcmp(r.rig.page.selected().name, "Headset") == 0);

    r.rig.page.handle(BtUiInput::Back);
    const BtUiRect dev0 = r.rect_of(BtUiAction::OpenDevice, 0);
    p = {true, dev0.x + 10, dev0.y + 5};
    r.poll(&p);
    p = {true, 5, 238};                                  // dragged off onto another control, then up
    r.poll(&p);
    p.down = false;
    r.poll(&p);
    CHECK(r.rig.page.screen() == BtUiScreen::List);     // cancelled
    p = {false, 0, 0};
    r.poll(&p);

    p = {true, 1, 200};                                  // empty list area / outside any control
    const BtUiRect close = r.rect_of(BtUiAction::Close);
    p = {true, close.x + 3, close.y + 3};
    r.poll(&p);
    p.down = false;
    r.poll(&p);
    CHECK(r.rig.page.closed());
}

void test_input_mouse_and_buttons() {
    BluetoothPageInputConfig cfg;
    cfg.buttons = {0, 1, 2, 3};
    InputRig r(cfg);
    r.poll();
    CHECK(r.input.capabilities() & bt_ui_input::Buttons);
    r.poll(nullptr, 1);                                  // next
    CHECK(r.rig.page.focus() == 1);
    r.poll(nullptr, 0);                                  // prev
    CHECK(r.rig.page.focus() == 0);
    r.poll(nullptr, 2);                                  // select
    CHECK(r.rig.page.screen() == BtUiScreen::Detail);
    r.poll(nullptr, 3);                                  // back
    CHECK(r.rig.page.screen() == BtUiScreen::List);
    r.poll(nullptr, 9);                                  // unmapped
    CHECK(r.rig.page.screen() == BtUiScreen::List);

    const BtUiRect dev = r.rect_of(BtUiAction::OpenDevice, 0);
    r.hid.mouse.present = true;
    r.hid.mouse.x = dev.x + 20;
    r.hid.mouse.y = dev.y + dev.h / 2;
    r.poll();
    CHECK(r.input.capabilities() & bt_ui_input::Pointer);
    r.hid.mouse.left = true;
    r.poll();
    r.hid.mouse.left = false;
    r.poll();
    CHECK(r.rig.page.screen() == BtUiScreen::Detail);
    r.hid.wheel = -1;                                    // wheel down = next
    r.poll();
    CHECK(r.rig.page.focus() == 1);
    r.hid.wheel = 1;
    r.poll();
    CHECK(r.rig.page.focus() == 0);
}

} // namespace

int run_bluetooth_ui_tests() {
    test_devices_merge();
    test_list_and_navigation();
    test_detail_and_forget();
    test_detail_follows_external_forget();
    test_pairing_success();
    test_pairing_failure_and_passkey();
    test_passkey_shown();
    test_back_stops_scan();
    test_focus_follows_item();
    test_layout_sizes();
    test_layout_scrolls_to_focus();
    test_view_render();
    test_input_keyboard();
    test_input_pad();
    test_input_touch_tap();
    test_input_mouse_and_buttons();
    return g_failures;
}
