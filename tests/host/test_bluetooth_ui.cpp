// Host tests for the common Bluetooth page: device merging and the page state machine.
#include <cstdio>
#include <cstring>
#include <vector>

#include "pico_toolset/bluetooth_page_controller.h"

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
    return g_failures;
}
