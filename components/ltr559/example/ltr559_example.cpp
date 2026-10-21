// LTR-559 smoke test: prints proximity and lux (Multi-Sensor Stick on
// Presto's QW/ST connector, I2C0 on GPIO40/41).
#include <cstdio>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico_toolset/ltr559.h"
#include "pico_toolset/ltr559_configs.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);
    i2c_init(i2c0, 400'000);
    gpio_set_function(40, GPIO_FUNC_I2C);
    gpio_set_function(41, GPIO_FUNC_I2C);
    gpio_pull_up(40);
    gpio_pull_up(41);

    pico_toolset::Ltr559 sensor;
    if (!sensor.init(pico_toolset::configs::ltr559::kPimoroniPrestoMultiSensorStick)) {
        std::printf("no LTR-559 at 0x23\n");
        return 1;
    }
    pico_toolset::Ltr559Reading r;
    while (true) {
        if (sensor.poll(r)) std::printf("prox %u  lux %.1f\n", r.proximity, r.lux);
        sleep_ms(200);
    }
}
