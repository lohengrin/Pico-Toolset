// ST7701 Palette8 smoke test at full 480x480 resolution: 225 KB framebuffer
// of colour indices, with the colour table animated every frame (a colour
// cycle needs no pixel writes at all). Presto preset, scanout on core1.
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico_toolset/st7701.h"
#include "pico_toolset/st7701_configs.h"

using namespace pico_toolset;

namespace {

constexpr int kSize = 480;
St7701 g_lcd;
uint8_t g_fb[kSize * kSize] __attribute__((aligned(4)));

void core1_main() {
    auto cfg = configs::st7701::kPimoroniPrestoFullRes;
    cfg.format = St7701Format::Palette8;
    cfg.framebuffer = g_fb;
    multicore_fifo_push_blocking(g_lcd.init(cfg) ? 1 : 0);
    while (true) tight_loop_contents();
}

} // namespace

int main() {
    stdio_init_all();
    multicore_launch_core1(core1_main);
    if (multicore_fifo_pop_blocking() != 1) return 1;

    // Diagonal gradient of palette indices.
    for (int y = 0; y < kSize; ++y)
        for (int x = 0; x < kSize; ++x) g_fb[y * kSize + x] = static_cast<uint8_t>((x + y) >> 2);

    uint8_t phase = 0;
    while (true) {
        for (int i = 0; i < 256; ++i) {
            const uint8_t k = static_cast<uint8_t>(i + phase);
            // Simple triangle-wave rainbow.
            auto wave = [](uint8_t v) -> uint8_t { return v < 128 ? v * 2 : (255 - v) * 2; };
            g_lcd.set_palette_colour(i, wave(k), wave(static_cast<uint8_t>(k + 85)),
                                     wave(static_cast<uint8_t>(k + 170)));
        }
        phase += 2;
        g_lcd.wait_for_vsync();
    }
}
