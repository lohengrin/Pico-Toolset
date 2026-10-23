#pragma once

#include <cstddef>
#include <cstdint>

#include "pico_toolset/hid_gamepad.h"

namespace pico_toolset {

namespace bt_hid_kind {
inline constexpr uint8_t Keyboard = 1, Mouse = 2, Gamepad = 4;
}

struct BtHidDevice {
    uint8_t addr[6];     // most significant byte first, as printed
    uint8_t addr_type;   // 0 public, 1 random
    uint8_t kinds;       // bt_hid_kind bits; 0 until the HID descriptor has been read
    int8_t  rssi;        // last seen while scanning, else 0
    bool    connected;
    char    name[24];
};

enum class BtHidEventType : uint8_t {
    PairingStarted,   // scan for HID devices begins
    PairingStopped,   // scan ended (timeout, stop_pairing(), or a device was picked)
    DeviceFound,      // a HID device advertised (device.rssi valid); call pair() unless auto_pair
    Connecting,       // link being set up to `device`
    PasskeyDisplay,   // show `passkey` to the user, who types it on the keyboard being paired
    Paired,           // bonded; keys are now stored
    Connected,        // HID service ready, reports flowing (device.kinds valid)
    Disconnected,
    PairingFailed,    // `status` holds the BTstack/HCI error
    Forgotten,
};

struct BtHidEvent {
    BtHidEventType type;
    BtHidDevice    device;
    uint32_t       passkey;   // PasskeyDisplay only
    uint8_t        status;    // PairingFailed only: BTstack/HCI error (0x05 = authentication failure)
    // DeviceFound of a Classic device only (0 otherwise): what the device reports about itself.
    uint32_t       class_of_device;  // Bluetooth class of device (major 0x05 = peripheral)
    uint16_t       vendor_id;        // Device ID profile, if the inquiry data carries one
    uint16_t       product_id;
    uint8_t        reason;    // PairingFailed only: SM reason code when status is 0x05 (e.g. 0x03 auth requirements, 0x05 pairing not supported, 0x04 passkey/oob), else 0
};

// Called from the BTstack/CYW43 context, not from an interrupt and not from
// the application thread: keep it short, only record state.
using BtHidCallback = void (*)(const BtHidEvent& event, void* user);

// Raw HID input report from a Classic device that could not be set up through BTstack's HID
// host (no usable SDP record), delivered from the BTstack context. `data` is the report
// payload without the 0xA1 transaction header. See BluetoothHidConfig::raw_report_callback.
using BtHidRawReportCallback = void (*)(const uint8_t* data, uint16_t len, const BtHidDevice& device, void* user);

// Behavior only -- the radio wiring comes from the board header.
struct BluetoothHidConfig {
    BtHidCallback callback = nullptr;
    void*    user = nullptr;
    bool     auto_reconnect = true;      // reconnect bonded devices after init and on disconnect
    bool     auto_pair = true;           // pair the first HID device found in pairing mode
    uint32_t pairing_timeout_ms = 30000; // pairing mode stops by itself after this
    // Bluetooth Classic (BR/EDR) HID: only effective when the build links BTstack's
    // classic support (CMake option PICO_TOOLSET_BLUETOOTH_CLASSIC).
    bool     enable_classic = true;
    const char* pin = "0000";            // answer to legacy PIN pairing requests (older pads)
    // Bonded Classic pads are also paged in the background. While we page we cannot answer a page,
    // and a pad that was switched on is paging us at that very time (its "searching" state), so the
    // first attempt waits to leave that window free and later ones are spread out with some jitter.
    uint32_t classic_page_delay_ms = 20000;
    uint32_t classic_page_interval_ms = 10000; // +-25 %
    // Some old pads have a broken SDP server, so BTstack's HID host cannot find their HID
    // channels. For those (after pairing) the board connects to the standard HID control and
    // interrupt channels itself and passes every input report here, unparsed.
    BtHidRawReportCallback raw_report_callback = nullptr;
    // Such pads have no readable descriptor either, so reports are parsed with this one (default: the
    // Android-style layout of the "SFR Gamepad", report id 3 = gamepad). Only reports whose first
    // byte equals raw_report_id are parsed (0 = all); the callback above sees every report.
    const uint8_t* raw_descriptor = nullptr;
    uint16_t       raw_descriptor_len = 0;
    uint8_t        raw_report_id = 3;
};

// BLE (and, when built in, Classic) HID host: connects keyboards, mice and gamepads (e.g. Xbox
// wireless controllers), pairs and bonds them, remembers them across reboots
// and reconnects. Bonding keys live in BTstack's TLV store in flash (set up by
// the pico-sdk's CYW43 BTstack port); this class additionally stores each
// device's address, kind and name so it can list and reconnect them.
//
// Requires Bluetooth::init() (components/bluetooth) to have run. Not usable
// together with Bluetooth::start_scan() at the same time (one GAP scan).
//
// FLASH: bonding writes flash from the BTstack context. With the other core
// running (e.g. LCD scanout), that core must execute only from RAM and the
// build must define PICO_FLASH_ASSUME_CORE1_SAFE=1, or flash_safe_execute()
// asserts -- see docs/components/bluetooth-hid.md.
class BluetoothHid {
public:
    static constexpr size_t kMaxConnected = 3;
    static constexpr size_t kMaxBonded = 4;

    bool init(const BluetoothHidConfig& config);

    // --- Pairing ---
    void start_pairing();
    void stop_pairing();
    [[nodiscard]] bool pairing() const { return m_pairing; }
    // For auto_pair == false: pair with a device reported by DeviceFound.
    bool pair(const uint8_t addr[6], uint8_t addr_type);

    // --- Bonded devices ---
    size_t bonded_devices(BtHidDevice* out, size_t max) const;
    bool forget(const uint8_t addr[6]);
    void forget_all();

    // --- Keyboard (usage IDs, as in UsbHidHost) ---
    [[nodiscard]] bool is_key_down(uint8_t hid_usage_id) const;
    [[nodiscard]] bool is_modifier_down(uint8_t mask) const; // bit 0 L-Ctrl .. bit 7 R-GUI
    bool consume_key_press(uint8_t hid_usage_id);

    // --- Mouse ---
    struct MouseState { bool present = false; bool left = false, right = false, middle = false; };
    [[nodiscard]] MouseState mouse_state() const;
    void consume_mouse_delta(int& dx, int& dy);
    int consume_mouse_wheel();

    // --- Gamepads (same GamepadState as UsbHidHost) ---
    [[nodiscard]] GamepadState gamepad_state(size_t index) const;
    // HID button usages 1..32 as a bitmask (bit n-1 = usage n), for checking or
    // remapping a controller whose layout differs from the built-in table.
    [[nodiscard]] uint32_t gamepad_raw_buttons(size_t index) const;

    [[nodiscard]] uint8_t connected_keyboard_count() const;
    [[nodiscard]] uint8_t connected_mouse_count() const;
    [[nodiscard]] uint8_t connected_gamepad_count() const;

    // Internal BTstack event entry points -- not for application use.
    struct Impl;
    void on_hci_event(uint8_t* packet);
    void on_sm_event(uint8_t* packet);
    void on_hids_event(uint8_t* packet);

private:
    friend struct Impl;
    BluetoothHidConfig m_cfg{};
    volatile bool m_pairing = false;
    bool m_initialised = false;
    Impl* m_impl = nullptr;
};

} // namespace pico_toolset
