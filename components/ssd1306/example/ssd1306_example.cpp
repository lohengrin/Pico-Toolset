// SSD1306 OLED driver example (doubles as an integration test).
// Displays a title, a counter frame, shapes, then toggles inversion.
#include "pico_toolset/ssd1306.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::Ssd1306;
using pico_toolset::Ssd1306Config;

int main() {
    stdio_init_all();
    sleep_ms(2000); // let the USB serial enumerate

    Ssd1306Config cfg;
    // Adapt pins here for your wiring:
    cfg.i2c_instance = i2c1;
    cfg.sda_pin      = 19;
    cfg.scl_pin      = 18;
    cfg.i2c_freq_hz  = 400000;
    cfg.address      = 0x3C;
    cfg.width        = 128;
    cfg.height       = 64;

    Ssd1306 disp;
    if (!disp.init(cfg)) {
        printf("SSD1306 init failed (framebuffer allocation)\n");
        return 1;
    }
    printf("SSD1306 init ok (%ux%u)\n", disp.width(), disp.height());

    char buf[32];
    uint32_t frame = 0;
    while (true) {
        disp.clear();
        disp.draw_string(0, 0, 1, "Pico-Toolset");
        disp.draw_empty_square(0, 16, 127, 16);
        snprintf(buf, sizeof(buf), "frame %lu", (unsigned long)frame);
        disp.draw_string(0, 20, 2, buf);
        disp.draw_square(110, 56, 16, 4);
        disp.show();
        sleep_ms(100);
        ++frame;
        if ((frame % 50) == 0)
            disp.invert((frame / 50) & 1); // blink inversion every 5 s
    }
}