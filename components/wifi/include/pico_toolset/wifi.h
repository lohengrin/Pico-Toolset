#pragma once

#include <cstdint>

namespace pico_toolset {

struct WifiConfig {
    const char* ssid = nullptr;
    const char* password = nullptr;
    // CYW43_AUTH_WPA2_AES_PSK (0x00400004), CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_OPEN, ...
    uint32_t auth = 0x00400004;
    uint32_t connect_timeout_ms = 30000;
    // Modem power saving (CYW43 PM2): the radio sleeps between traffic bursts
    // and wakes on beacons/data. Large idle-current win on a Pico W.
    bool power_save = true;
};

// Initialises the CYW43 chip in station mode and joins an access point
// (blocking). Pico W / Pico 2 W only.
// Requires linking pico_cyw43_arch_lwip_threadsafe_background (done by the target).
bool wifi_connect(const WifiConfig& config);

} // namespace pico_toolset
