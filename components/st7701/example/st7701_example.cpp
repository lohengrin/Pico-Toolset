// ST7701 smoke test: colour bars with an animated backlight ramp. Scanout
// runs on core1 so core0 can stall on flash/PSRAM without glitching the
// panel. Half-resolution Presto preset; defaults match pico_toolset's
// configs::st7701::kPimoroniPrestoHalfRes.
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico_toolset/st7701.h"
#include "pico_toolset/st7701_configs.h"

using namespace pico_toolset;

namespace {

St7701 g_lcd;
uint16_t g_fb[240 * 240] __attribute__((aligned(4)));
uint16_t g_frame[240 * 240];

void core1_main() {
    auto cfg = configs::st7701::kPimoroniPrestoHalfRes;
    cfg.framebuffer = g_fb;
    multicore_fifo_push_blocking(g_lcd.init(cfg) ? 1 : 0);
    while (true) tight_loop_contents();
}

} // namespace

int main() {
    stdio_init_all();
    multicore_launch_core1(core1_main);
    if (multicore_fifo_pop_blocking() != 1) return 1;

    constexpr uint16_t bars[] = {0xF800, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0xF81F, 0xFFFF, 0x0000};
    for (int y = 0; y < 240; ++y)
        for (int x = 0; x < 240; ++x)
            g_frame[y * 240 + x] = st7701_encode(bars[x / 30]);
    g_lcd.present(g_frame);

    uint8_t level = 0;
    int step = 5;
    while (true) {
        g_lcd.set_backlight(level);
        level += step;
        if (level >= 250 || level <= 5) step = -step;
        sleep_ms(20);
    }
}
