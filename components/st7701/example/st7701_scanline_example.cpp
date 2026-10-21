// ST7701 Scanline565 smoke test: no framebuffer at all. A callback renders
// each line just ahead of the beam (here: colour bars plus a bouncing bar,
// computed per line), so the whole display costs a few KB of SRAM at any
// resolution. The line callback and its data live in RAM (__not_in_flash_func).
// Presto preset at 480x480, scanout and rendering on core1.
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "pico_toolset/st7701.h"
#include "pico_toolset/st7701_configs.h"

#include <cstdio>

using namespace pico_toolset;

namespace {

constexpr int kSize = 480;
St7701 g_lcd;
volatile int g_bar_y = 100;

constexpr uint16_t kBars[8] = {0xF800, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0xF81F, 0xFFFF, 0x0000};

// Pre-rendered source lines: the callback only has about one line time
// (~28 us at the dot clock chosen below), so it copies 32 bits at a time
// instead of computing pixels.
uint32_t g_bar_line[kSize / 2] __attribute__((aligned(4)));
uint32_t g_white_line[kSize / 2] __attribute__((aligned(4)));

void __not_in_flash_func(render_line)(int y, uint16_t* line, void*) {
    const int bar = g_bar_y;
    const uint32_t* src = (y >= bar && y < bar + 40) ? g_white_line : g_bar_line;
    uint32_t* dst = reinterpret_cast<uint32_t*>(line);
    for (int i = 0; i < kSize / 2; ++i) dst[i] = src[i];
}

void core1_main() {
    uint16_t* bar = reinterpret_cast<uint16_t*>(g_bar_line);
    uint16_t* white = reinterpret_cast<uint16_t*>(g_white_line);
    for (int x = 0; x < kSize; ++x) {
        bar[x] = st7701_encode(kBars[x / (kSize / 8)]);
        white[x] = 0xFFFF;
    }
    auto cfg = configs::st7701::kPimoroniPrestoFullRes;
    cfg.max_pixel_clock_hz = 20'000'000; // ~19 MHz at 150 MHz sysclk: ~28 us per line
    cfg.format = St7701Format::Scanline565;
    cfg.line_callback = render_line;
    multicore_fifo_push_blocking(g_lcd.init(cfg) ? 1 : 0);
    while (true) g_lcd.scanline_task();
}

} // namespace

int main() {
    stdio_init_all();
    multicore_launch_core1(core1_main);
    if (multicore_fifo_pop_blocking() != 1) return 1;

    int dir = 3;
    while (true) {
        int y = g_bar_y + dir;
        if (y < 0 || y > kSize - 40) dir = -dir;
        g_bar_y = y;
        sleep_ms(16);
        static int n = 0;
        if (++n % 120 == 0)
            std::printf("missed frames %lu, late lines %lu\n",
                        static_cast<unsigned long>(g_lcd.missed_frames()),
                        static_cast<unsigned long>(g_lcd.late_lines()));
    }
}
