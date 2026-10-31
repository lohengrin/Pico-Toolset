#pragma once

#include "lvgl.h"

#include "pico_toolset/bluetooth_page_controller.h"

namespace pico_toolset {

struct BluetoothPageLvglConfig {
    const char* title = "Bluetooth";           // may start with an LV_SYMBOL_*
    const lv_font_t* title_font = nullptr;     // null = the theme's font
    const lv_font_t* body_font = nullptr;
    const lv_font_t* passkey_font = nullptr;   // the code a keyboard asks to be typed
    int32_t pad = 12;
    int32_t row_height = 44;
    int32_t button_height = 40;
};

// Draws a BluetoothPageController with LVGL widgets: a header, a status line, the list (or the
// device detail / forget question), and the buttons, with the same screens, texts and actions as
// the DisplayDriver view (BluetoothPageView). Taps and clicks reach the controller through LVGL's
// own events; a keyboard, pad or encoder navigates through the page's lv_group (arrows / Tab / pad =
// LV_KEY_PREV / NEXT, Enter = click, Esc or Backspace = back), so the application only has to point
// its keypad indev at group() while the page is shown.
//
//   BluetoothPageController controller(devices);
//   BluetoothPageLvgl page(controller, {.title = LV_SYMBOL_BLUETOOTH " Bluetooth"});
//   controller.open();
//   page.create(lv_layer_top());                   // fills the parent
//   lv_indev_set_group(keypad, page.group());
//   ... every few tens of ms:  page.update();      // call lv_timer_handler() as usual
//   ... when controller.closed():  page.destroy();
class BluetoothPageLvgl {
public:
    explicit BluetoothPageLvgl(BluetoothPageController& controller, const BluetoothPageLvglConfig& config = {})
        : page_(controller), cfg_(config) {}
    ~BluetoothPageLvgl() { destroy(); }
    BluetoothPageLvgl(const BluetoothPageLvgl&) = delete;
    BluetoothPageLvgl& operator=(const BluetoothPageLvgl&) = delete;

    // Builds the page inside `parent` (sized to fill it) and its focus group. Returns the root object.
    lv_obj_t* create(lv_obj_t* parent);
    void destroy();
    [[nodiscard]] bool created() const { return root_ != nullptr; }
    [[nodiscard]] lv_obj_t* root() const { return root_; }
    [[nodiscard]] lv_group_t* group() const { return group_; }

    // Updates the controller and what is shown. The widgets are only rebuilt when the content
    // changes, so a tap or a focus is not lost to a scan result. True when something changed.
    bool update();

private:
    static void on_item_event(lv_event_t* e);
    static void on_root_key(lv_event_t* e);
    void build_items();
    void refresh_header();
    uint32_t signature() const;
    void sync_focus();

    BluetoothPageController& page_;
    BluetoothPageLvglConfig cfg_;
    lv_obj_t* root_ = nullptr;
    lv_obj_t* title_ = nullptr;
    lv_obj_t* status_ = nullptr;
    lv_obj_t* passkey_ = nullptr;
    lv_obj_t* list_ = nullptr;
    lv_obj_t* footer_ = nullptr;
    lv_group_t* group_ = nullptr;
    lv_obj_t* items_[BluetoothPageController::kMaxItems] = {};
    uint32_t signature_ = 0;
    bool building_ = false;
};

} // namespace pico_toolset
