// ST7789 SPI LCD example (doubles as an integration test).
// Fills the panel with a sequence of solid colors.
#include "pico_toolset/st7789.h"
#include "pico_toolset/st7789_configs.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::St7789;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // On a different board, copy this preset and change only the fields
    // that differ, or build an St7789Config from scratch -- see that
    // struct's own doc comment for which fields you must set.
    St7789 lcd;
    if (!lcd.init(pico_toolset::configs::st7789::kElecrowCrowPanelPicoHmi28)) {
        printf("ST7789 init failed\n");
        return 1;
    }
    printf("ST7789 init ok (%dx%d)\n", lcd.width(), lcd.height());

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
