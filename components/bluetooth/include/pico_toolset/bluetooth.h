#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

struct BluetoothConfig {
    const char* name = "pico-toolset"; // GAP/advertised local name (<= 29 characters)
    uint16_t adv_interval_ms = 100;    // advertising interval
    uint16_t scan_interval_ms = 30;    // scan window == interval (continuous scan)
    bool     active_scan = true;       // ask advertisers for scan responses (names)
};

struct BleAdvertisement {
    uint8_t addr[6];     // most significant byte first, as printed
    uint8_t addr_type;   // 0 public, 1 random
    int8_t  rssi;
    char    name[32];    // empty if the advertisement carries none
};

// Called from the BTstack/CYW43 context (not an interrupt): keep it short and
// do not call blocking Bluetooth APIs from it.
using BleScanCallback = void (*)(const BleAdvertisement& adv, void* user);

// BLE broadcaster + observer (non-connectable advertising, scanning) over
// BTstack on the CYW43 radio. No GATT. Requires a board with the CYW43
// (Pico W / Pico 2 W / Presto), and cyw43_arch_init() must already have run --
// wifi_connect() does that, or call it yourself.
class Bluetooth {
public:
    // Brings BTstack up and waits until the controller is working.
    // False on timeout.
    bool init(const BluetoothConfig& config);

    void start_advertising();
    void stop_advertising();

    void start_scan(BleScanCallback callback, void* user = nullptr);
    void stop_scan();

    [[nodiscard]] bool ready() const { return m_ready; }

    // Event entry point -- not for application use.
    void on_event(uint8_t packet_type, const uint8_t* packet);

private:
    BluetoothConfig m_cfg{};
    BleScanCallback m_scan_cb = nullptr;
    void* m_scan_user = nullptr;
    volatile bool m_ready = false;
    bool m_initialised = false;
};

} // namespace pico_toolset
