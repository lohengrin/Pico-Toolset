// Headless smoke test of BluetoothPageLvgl: the same controller as the pixel view, drawn with LVGL
// objects. Checks structure and behaviour (items, taps, focus, keys, no needless rebuilds), not pixels.
#include <cstdio>
#include <cstring>

#include "fake_bt_backend.h"
#include "pico_toolset/bluetooth_page_lvgl.h"

using namespace pico_toolset;

static int g_failures = 0;
#define CHECK(cond)                                                                       \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);                   \
            ++g_failures;                                                                 \
        }                                                                                 \
    } while (0)

namespace {

uint32_t g_ms = 0;
uint32_t tick() { return g_ms; }
void flush(lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); }

// Finds a label whose text contains `needle` anywhere below `root`.
lv_obj_t* find_label(lv_obj_t* root, const char* needle) {
    const uint32_t n = lv_obj_get_child_count(root);
    for (uint32_t i = 0; i < n; ++i) {
        lv_obj_t* c = lv_obj_get_child(root, static_cast<int32_t>(i));
        if (lv_obj_check_type(c, &lv_label_class) && std::strstr(lv_label_get_text(c), needle)) return c;
        if (lv_obj_t* f = find_label(c, needle)) return f;
    }
    return nullptr;
}

void key(lv_obj_t* target, uint32_t k) { lv_obj_send_event(target, LV_EVENT_KEY, &k); }

struct Rig {
    FakeBackend backend{BtUiSource::Hid, false};
    BluetoothDevices devices;
    BluetoothPageController controller{devices};
    BluetoothPageLvgl page{controller};
    Rig() {
        devices.add_backend(backend);
        backend.paired = {make_device(BtUiSource::Hid, 1, "Keys", BtUiKind::Keyboard, BtUiState::Connected),
                          make_device(BtUiSource::Hid, 2, "Mouse", BtUiKind::Mouse)};
        controller.open();
        page.create(lv_screen_active());
    }
};

void test_list_and_detail() {
    Rig r;
    CHECK(r.page.created());
    // two devices + PAIR NEW + CLOSE, all in the focus group
    CHECK(r.controller.item_count() == 4);
    CHECK(lv_group_get_obj_count(r.page.group()) == 4);
    CHECK(find_label(r.page.root(), "Keys") != nullptr);
    CHECK(find_label(r.page.root(), "PAIR NEW") != nullptr);
    CHECK(find_label(r.page.root(), "Bluetooth (2)") != nullptr);

    // a tap on the first device opens its detail screen
    r.controller.activate(0);
    r.page.update();
    CHECK(r.controller.screen() == BtUiScreen::Detail);
    CHECK(find_label(r.page.root(), "Kind: Keyboard") != nullptr);
    CHECK(find_label(r.page.root(), "FORGET") != nullptr);

    // Esc on the focused object goes back
    key(lv_group_get_focused(r.page.group()), LV_KEY_ESC);
    r.page.update();
    CHECK(r.controller.screen() == BtUiScreen::List);
}

void test_focus_follows() {
    Rig r;
    CHECK(r.controller.focus() == 0);
    lv_group_focus_next(r.page.group());
    r.page.update();
    CHECK(r.controller.focus() == 1);
    lv_group_focus_next(r.page.group());
    CHECK(r.controller.focus() == 2);
    // the controller moving the focus (a rebuilt list) is followed by the group
    r.controller.set_focus(0);
    r.page.update();
    CHECK(lv_group_get_focused(r.page.group()) == lv_obj_get_child(lv_obj_get_child(r.page.root(), 3), 0));
}

void test_scan_pair_and_no_needless_rebuild() {
    Rig r;
    r.controller.activate(2); // PAIR NEW
    r.page.update();
    CHECK(r.controller.screen() == BtUiScreen::Scan);
    CHECK(find_label(r.page.root(), "Looking for devices") != nullptr);
    CHECK(find_label(r.page.root(), "Scanning") != nullptr);

    r.backend.seen = {make_device(BtUiSource::Hid, 7, "New pad", BtUiKind::Gamepad)};
    ++r.backend.counter;
    r.page.update();
    lv_obj_t* label = find_label(r.page.root(), "New pad");
    CHECK(label != nullptr);
    lv_obj_t* row = lv_obj_get_parent(label);

    // the same signal strength reported again changes nothing: the row object survives
    ++r.backend.counter;
    r.page.update();
    CHECK(lv_obj_get_parent(find_label(r.page.root(), "New pad")) == row);

    // a tap on the found device pairs it and the page returns to the list with a notice
    r.controller.activate(0);
    r.page.update();
    CHECK(r.controller.screen() == BtUiScreen::List);
    CHECK(find_label(r.page.root(), "New pad") != nullptr);
    CHECK(find_label(r.page.root(), "Paired") != nullptr);
}

void test_passkey_and_forget() {
    Rig r;
    lv_obj_t* passkey = nullptr;
    r.backend.passkey_value = 123456;
    ++r.backend.counter;
    r.page.update();
    passkey = find_label(r.page.root(), "123456");
    CHECK(passkey != nullptr && !lv_obj_has_flag(passkey, LV_OBJ_FLAG_HIDDEN));
    CHECK(find_label(r.page.root(), "Type 123456 on the device") != nullptr);
    r.backend.passkey_value = 0;
    ++r.backend.counter;
    r.page.update();
    CHECK(lv_obj_has_flag(find_label(r.page.root(), "123456") ? find_label(r.page.root(), "123456") : passkey, LV_OBJ_FLAG_HIDDEN)
          || find_label(r.page.root(), "123456") == nullptr);

    r.controller.activate(1); // Mouse
    r.page.update();
    CHECK(find_label(r.page.root(), "CONNECT") != nullptr); // offline
    r.controller.handle(BtUiInput::Next); // Forget
    r.controller.handle(BtUiInput::Select);
    r.page.update();
    CHECK(r.controller.screen() == BtUiScreen::ConfirmForget);
    CHECK(find_label(r.page.root(), "Forget Mouse?") != nullptr);
    CHECK(find_label(r.page.root(), "YES") != nullptr);
    r.controller.activate(0);
    r.page.update();
    CHECK(r.backend.forgets == 1);
    CHECK(r.backend.paired.size() == 1);
}

void test_close_and_recreate() {
    Rig r;
    r.controller.activate(3); // CLOSE
    CHECK(r.controller.closed());
    r.page.destroy();
    CHECK(!r.page.created());
    r.controller.open();
    r.page.create(lv_screen_active());
    CHECK(r.page.created() && !r.controller.closed());
    CHECK(lv_group_get_obj_count(r.page.group()) == 4);
}

} // namespace

int main() {
    lv_init();
    lv_tick_set_cb(tick);
    lv_display_t* d = lv_display_create(480, 480);
    static uint8_t buf[480 * 40 * 2];
    lv_display_set_buffers(d, buf, nullptr, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(d, flush);

    test_list_and_detail();
    test_focus_follows();
    test_scan_pair_and_no_needless_rebuild();
    test_passkey_and_forget();
    test_close_and_recreate();

    g_ms += 100;
    lv_timer_handler(); // lays out and renders once: catches style / layout crashes
    if (g_failures == 0) std::printf("bluetooth LVGL page: all tests passed\n");
    return g_failures ? 1 : 0;
}
