// ILI9486 SPI LCD example (doubles as an integration test).
// Fills the panel with a sequence of solid colors plus a simple color sweep.
#include "pico_toolset/ili9486.h"
#include "pico_toolset/ili9486_configs.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::Ili9486;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // On a different board, copy this preset and change only the fields
    // that differ, or build an Ili9486Config from scratch -- see that
    // struct's own doc comment for which fields you must set.
    Ili9486 lcd;
    if (!lcd.init(pico_toolset::configs::ili9486::kWaveshareRp2350PiZero)) {
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