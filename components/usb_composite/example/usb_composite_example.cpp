// Smoke test: 64 KiB RAM disk exposed over MSC (host sees an unformatted
// drive to format) plus an echo on the CDC port.
#include "pico_toolset/usb_composite.h"

#include "pico/stdlib.h"

#include <cstdio>
#include <cstring>

namespace {
constexpr uint32_t kBlocks = 128;
uint8_t g_ram[kBlocks * 512];

bool ready(void*) { return true; }
uint32_t count(void*) { return kBlocks; }
bool rd(void*, uint32_t lba, uint8_t* b, uint32_t n) {
    if (lba + n > kBlocks) return false;
    memcpy(b, g_ram + lba * 512, n * 512);
    return true;
}
bool wr(void*, uint32_t lba, const uint8_t* b, uint32_t n) {
    if (lba + n > kBlocks) return false;
    memcpy(g_ram + lba * 512, b, n * 512);
    return true;
}
} // namespace

int main() {
    stdio_init_all();
    pico_toolset::UsbCompositeConfig cfg;
    cfg.block_device = {nullptr, ready, count, rd, wr};
    pico_toolset::usb_composite_init(cfg);
    while (true) {
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) putchar(c);
        pico_toolset::usb_composite_task();
    }
}
