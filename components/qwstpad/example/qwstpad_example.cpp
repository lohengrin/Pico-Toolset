// QwSTPad smoke test: prints pressed buttons and mirrors A/B/X/Y on the
// four LEDs. Presto QW/ST preset (I2C0 on GPIO40/41).
#include <cstdio>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico_toolset/qwstpad.h"
#include "pico_toolset/qwstpad_configs.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);
    i2c_init(i2c0, 400'000);
    gpio_set_function(40, GPIO_FUNC_I2C);
    gpio_set_function(41, GPIO_FUNC_I2C);
    gpio_pull_up(40);
    gpio_pull_up(41);

    pico_toolset::QwstPad pad;
    if (!pad.init(pico_toolset::configs::qwstpad::kPimoroniPresto)) {
        std::printf("no QwSTPad at 0x21\n");
        return 1;
    }
    using namespace pico_toolset::qwstpad_button;
    uint16_t prev = 0;
    while (true) {
        const uint16_t b = pad.read_buttons();
        if (b != prev) std::printf("buttons 0x%03X\n", b);
        prev = b;
        pad.set_leds((b & A ? 1 : 0) | (b & B ? 2 : 0) | (b & X ? 4 : 0) | (b & Y ? 8 : 0));
        sleep_ms(10);
    }
}
