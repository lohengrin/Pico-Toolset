// ILI9486 SPI LCD example (doubles as an integration test).
// Fills the panel with a sequence of solid colors plus a simple color sweep.
#include "pico_toolset/ili9486.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::Ili9486;
using pico_toolset::Ili9486Config;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    Ili9486Config cfg;
    // Adapt pins here for your board:
    cfg.spi_instance = spi1;
    cfg.pin_sck      = 10;
    cfg.pin_mosi     = 11;
    cfg.pin_miso     = 12;
    cfg.pin_cs       = 8;
    cfg.pin_dc       = 24;
    cfg.pin_rst      = 25;
    cfg.pin_backlight = 255; // or a PWM-capable GPIO for dimming
    cfg.use_dma      = true;

    Ili9486 lcd;
    if (!lcd.init(cfg)) {
        printf("ILI9486 init failed\n");
        return 1;
    }
    printf("ILI9486 init ok (%dx%d)\n", Ili9486::kWidth, Ili9486::kHeight);

    constexpr uint16_t colors[] = {
        0xF800, // red
        0x07E0, // green
        0x001F, // blue
        0xFFFF, // white
        0x0000, // black
    };

    while (true) {
        for (uint16_t c : colors) {
            lcd.fill_solid(c);
            sleep_ms(500);
        }
    }
}