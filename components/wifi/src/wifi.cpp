#include "pico_toolset/wifi.h"

#include <cstdio>

#include "pico/cyw43_arch.h"

static_assert(CYW43_AUTH_WPA2_AES_PSK == 0x00400004, "WifiConfig::auth default out of sync");

namespace pico_toolset {

bool wifi_connect(const WifiConfig& config) {
    if (!config.ssid) {
        return false;
    }
    if (cyw43_arch_init()) {
        std::printf("cyw43 init failed\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    if (cyw43_arch_wifi_connect_timeout_ms(config.ssid, config.password, config.auth,
                                           config.connect_timeout_ms)) {
        std::printf("wifi connect failed\n");
        return false;
    }
    std::printf("wifi connected\n");

    if (config.power_save) {
        cyw43_wifi_pm(&cyw43_state, cyw43_pm_value(CYW43_PM2_POWERSAVE_MODE, 200, 1, 1, 1));
    }
    return true;
}

} // namespace pico_toolset
