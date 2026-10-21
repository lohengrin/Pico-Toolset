// BLE smoke test: advertises as "pico-toolset" and prints every advertiser
// seen. Needs a CYW43 board (e.g. PICO_BOARD=pimoroni_presto or pico2_w).
#include <cstdio>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico_toolset/bluetooth.h"

static void on_adv(const pico_toolset::BleAdvertisement& a, void*) {
    std::printf("%02X:%02X:%02X:%02X:%02X:%02X  rssi %d  %s\n", a.addr[0], a.addr[1], a.addr[2],
                a.addr[3], a.addr[4], a.addr[5], a.rssi, a.name);
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    if (cyw43_arch_init()) {
        std::printf("cyw43 init failed\n");
        return 1;
    }
    pico_toolset::Bluetooth bt;
    if (!bt.init({})) {
        std::printf("bluetooth init timed out\n");
        return 1;
    }
    bt.start_advertising();
    bt.start_scan(on_adv);
    while (true) sleep_ms(1000);
}
