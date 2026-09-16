// ST7796U SPI LCD example (doubles as an integration test).
// Fills the panel with a sequence of solid colors.
#include "pico_toolset/st7796.h"
#include "pico_toolset/st7796_configs.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::St7796;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // On a different board, copy this preset and change only the fields
    // that differ, or build an St7796Config from scratch -- see that
    // struct's own doc comment for which fields you must set.
    St7796 lcd;
    if (!lcd.init(pico_toolset::configs::st7796::kWaveshareRp2350PiZero)) {
        printf("ST7796U init failed\n");
        return 1;
    }
    printf("ST7796U init ok (%dx%d)\n", lcd.width(), lcd.height());

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
