// XPT2046 touch controller example (doubles as an integration test).
// Prints raw ADC readings whenever the panel is touched. This component
// never calls spi_init() itself (see the class doc comment) -- it shares a
// bus a display driver already brought up, so this example brings the bus
// up directly rather than depending on pico_toolset_ili9486.
#include "pico_toolset/xpt2046.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::Xpt2046Config;
using pico_toolset::Xpt2046Touch;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // Adapt pins/SPI instance here for your board -- must match whatever
    // display driver (or your own spi_init() call) already owns this bus.
    spi_inst_t* const kSpi = spi1;
    constexpr uint kPinSck = 10;
    constexpr uint kPinMosi = 11;
    constexpr uint kPinMiso = 12;

    spi_init(kSpi, 2'000'000);
    gpio_set_function(kPinSck, GPIO_FUNC_SPI);
    gpio_set_function(kPinMosi, GPIO_FUNC_SPI);
    gpio_set_function(kPinMiso, GPIO_FUNC_SPI);

    Xpt2046Config cfg;
    cfg.spi_instance = kSpi;
    cfg.pin_cs = 7;
    cfg.pin_irq = 17;
    cfg.touch_freq_hz = 2'000'000;

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
