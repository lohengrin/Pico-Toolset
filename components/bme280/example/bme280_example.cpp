// BME280 smoke test: prints temperature, pressure and humidity every second
// (Multi-Sensor Stick on Presto's QW/ST connector, I2C0 on GPIO40/41).
#include <cstdio>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico_toolset/bme280.h"
#include "pico_toolset/bme280_configs.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);
    i2c_init(i2c0, 400'000);
    gpio_set_function(40, GPIO_FUNC_I2C);
    gpio_set_function(41, GPIO_FUNC_I2C);
    gpio_pull_up(40);
    gpio_pull_up(41);

    pico_toolset::Bme280 sensor;
    if (!sensor.init(pico_toolset::configs::bme280::kPimoroniPrestoMultiSensorStick)) {
        std::printf("no BME280 at 0x76\n");
        return 1;
    }
    while (true) {
        auto r = sensor.read();
        if (r.valid)
            std::printf("%.2f C  %.1f hPa  %.1f %%RH\n", r.temperature_c, r.pressure_pa / 100.0f, r.humidity_pct);
        sleep_ms(1000);
    }
}
