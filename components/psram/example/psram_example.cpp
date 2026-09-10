// PSRAM driver example (doubles as an integration test). RP2350 only.
// Detects the PSRAM, runs the self-test, then exercises the allocator and
// the std::pmr adapter.
#include "pico_toolset/psram.h"
#include "pico/stdlib.h"

#include <cstdio>
#include <vector>

using pico_toolset::PsramConfig;
using pico_toolset::PsramResource;
using pico_toolset::psram_init;
using pico_toolset::psram_malloc;
using pico_toolset::psram_free;
using pico_toolset::psram_status;
using pico_toolset::psram_used_bytes;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    PsramConfig cfg;
    cfg.cs_pin = 47; // adjust to your board (Waveshare RP2350-PiZero uses GPIO47)
    cfg.run_self_test = true;

    auto status = psram_init(cfg);
    printf("PSRAM present=%d test_ok=%d size=%zu bytes at clk_sys=%u Hz\n",
           status.present, status.test_ok, status.size_bytes,
           status.clk_sys_hz_at_test);
    if (!status.test_ok) {
        printf("PSRAM FAILED: offset=%zu expected=0x%02X actual=0x%02X\n",
               status.fail_offset, status.fail_expected, status.fail_actual);
        return 1;
    }

    // Raw allocator check.
    auto* a = psram_malloc(1024);
    auto* b = psram_malloc(4096);
    if (!a || !b) {
        printf("PSRAM malloc failed\n");
        return 1;
    }
    for (int i = 0; i < 1024; ++i)
        static_cast<volatile uint8_t*>(a)[i] = static_cast<uint8_t>(i & 0xFF);
    bool ok = true;
    for (int i = 0; i < 1024; ++i)
        if (static_cast<volatile uint8_t*>(a)[i] != static_cast<uint8_t>(i & 0xFF)) ok = false;
    psram_free(a);
    psram_free(b);
    printf("PSRAM allocator read/write %s, used=%zu\n", ok ? "PASS" : "FAIL", psram_used_bytes());

    // std::pmr adapter check.
    PsramResource resource;
    std::pmr::vector<uint8_t> v(&resource);
    for (int i = 0; i < 100000; ++i)
        v.push_back(static_cast<uint8_t>(i & 0xFF));
    bool vok = true;
    for (int i = 0; i < 100000; ++i)
        if (v[i] != static_cast<uint8_t>(i & 0xFF)) vok = false;
    printf("PSRAM pmr::vector %s, size=%zu used=%zu\n",
           vok ? "PASS" : "FAIL", v.size(), psram_used_bytes());

    printf("PSRAM example done\n");
    return ok && vok ? 0 : 1;
}