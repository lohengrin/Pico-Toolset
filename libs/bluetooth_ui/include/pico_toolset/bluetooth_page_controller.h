#pragma once

#include "pico_toolset/bluetooth_devices.h"

namespace pico_toolset {

enum class BtUiScreen : uint8_t { List, Scan, Detail, ConfirmForget };

// What activating an item does. The view draws one control per item (a row or a button) in the
// order given and decides where it goes on the screen; the controller owns what it means.
enum class BtUiAction : uint8_t {
    OpenDevice,     // index = bonded device: open its detail screen
    PickFound,      // index = found device: pair / connect it
    StartScan,
    StopScan,
    Close,          // leave the page
    Back,
    Connect,
    Disconnect,
    Forget,
    ConfirmForget,
    Cancel,
};

struct BtUiItem {
    BtUiAction action;
    int8_t     index;  // device index for OpenDevice / PickFound, else -1
};

// Abstract input, produced by the touch / pointer / keyboard / gamepad / button adaptors.
enum class BtUiInput : uint8_t { Prev, Next, Select, Back };

enum class BtUiNotice : uint8_t { None, Paired, PairFailed, Forgotten };

// The page's state machine. No drawing and no hardware: the view reads screen(), items(), the device
// snapshots and status, and feeds input back in.
class BluetoothPageController {
public:
    static constexpr size_t kMaxItems = BluetoothDevices::kMaxDevices + BluetoothDevices::kMaxFound + 3;

    explicit BluetoothPageController(BluetoothDevices& devices) : devices_(devices) {}

    void open();                       // show the bonded list again
    [[nodiscard]] bool closed() const { return closed_; }

    // Call regularly (a few times a second is plenty). True when something visible changed.
    bool update();
    void handle(BtUiInput input);
    void activate(size_t item);        // a tap / click on an item

    [[nodiscard]] BtUiScreen screen() const { return screen_; }
    [[nodiscard]] size_t item_count() const { return item_count_; }
    [[nodiscard]] BtUiItem item(size_t i) const { return items_[i]; }
    [[nodiscard]] size_t focus() const { return focus_; }
    void set_focus(size_t item);

    [[nodiscard]] size_t bonded_count() const { return bonded_count_; }
    [[nodiscard]] const BtUiDevice& bonded(size_t i) const { return bonded_[i]; }
    [[nodiscard]] size_t found_count() const { return found_count_; }
    [[nodiscard]] const BtUiDevice& found(size_t i) const { return found_[i]; }
    [[nodiscard]] const BtUiDevice& selected() const { return detail_; } // Detail / ConfirmForget

    [[nodiscard]] bool scanning() const { return scanning_; }
    [[nodiscard]] bool connecting() const { return connecting_; }
    bool passkey(uint32_t* code) const;
    [[nodiscard]] BtUiNotice notice() const { return notice_; }
    [[nodiscard]] uint8_t error() const { return error_; }  // PairFailed: BTstack/HCI status

private:
    void refresh();
    void rebuild_items(bool keep_focus);
    void go(BtUiScreen screen);
    void run(BtUiItem item);
    void back();

    BluetoothDevices& devices_;
    BtUiScreen screen_ = BtUiScreen::List;
    bool closed_ = false;
    bool dirty_ = true;
    uint32_t last_counter_ = 0;
    bool have_counter_ = false;

    BtUiDevice bonded_[BluetoothDevices::kMaxDevices]{};
    size_t bonded_count_ = 0;
    BtUiDevice found_[BluetoothDevices::kMaxFound]{};
    size_t found_count_ = 0;
    BtUiDevice detail_{};
    BtUiItem items_[kMaxItems]{};
    size_t item_count_ = 0;
    size_t focus_ = 0;

    bool scanning_ = false;
    bool connecting_ = false;
    bool has_passkey_ = false;
    uint32_t passkey_ = 0;
    BtUiNotice notice_ = BtUiNotice::None;
    uint8_t error_ = 0;
    bool pending_ = false;     // a found device was picked and its result is awaited
    BtUiDevice pending_device_{};
};

} // namespace pico_toolset
