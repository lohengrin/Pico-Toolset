// XPT2046 touch controller example (doubles as an integration test).
// Prints raw ADC readings whenever the panel is touched. This component
// never calls spi_init() itself (see the class doc comment) -- it shares a
// bus a display driver already brought up, so this example brings the bus
// up directly rather than depending on pico_toolset_ili9486.
#include "pico_toolset/xpt2046.h"
#include "pico_toolset/xpt2046_configs.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::Xpt2046Touch;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // On a different board, copy this preset and change only the fields
    // that differ (spi_instance especially: it must match whatever display
    // driver, or your own spi_init() call, already owns this bus).
    spi_inst_t* const kSpi = spi1;
    constexpr uint kPinSck = 10;
    constexpr uint kPinMosi = 11;
    constexpr uint kPinMiso = 12;

    spi_init(kSpi, 2'000'000);
    gpio_set_function(kPinSck, GPIO_FUNC_SPI);
    gpio_set_function(kPinMosi, GPIO_FUNC_SPI);
    gpio_set_function(kPinMiso, GPIO_FUNC_SPI);

    auto cfg = pico_toolset::configs::xpt2046::kWaveshareRp2350PiZero;
    cfg.spi_instance = kSpi;

    Xpt2046Touch touch;
    touch.init(cfg);
    printf("XPT2046 init ok\n");

    bool was_pressed = false;
    while (true) {
        Xpt2046Touch::RawSample sample = touch.read();
        if (sample.pressed && !was_pressed) {
            printf("pressed raw_x=%u raw_y=%u\n", sample.raw_x, sample.raw_y);
        } else if (!sample.pressed && was_pressed) {
            printf("released\n");
        }
        was_pressed = sample.pressed;
        sleep_ms(20);
    }
}
