// LSM6DS3 smoke test: prints accel (g) and gyro (dps) at 5 Hz plus the step
// count. Multi-Sensor Stick on Presto's QW/ST connector (I2C0, GPIO40/41).
#include <cstdio>

#include "hardware/gpio.h"
#include "hardware/i2c.h"
#include "pico/stdlib.h"
#include "pico_toolset/lsm6ds3.h"
#include "pico_toolset/lsm6ds3_configs.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);
    i2c_init(i2c0, 400'000);
    gpio_set_function(40, GPIO_FUNC_I2C);
    gpio_set_function(41, GPIO_FUNC_I2C);
    gpio_pull_up(40);
    gpio_pull_up(41);

    pico_toolset::Lsm6ds3 imu;
    if (!imu.init(pico_toolset::configs::lsm6ds3::kPimoroniPrestoMultiSensorStick)) {
        std::printf("no LSM6DS3 at 0x6A\n");
        return 1;
    }
    std::printf("WHO_AM_I 0x%02X\n", imu.who_am_i());
    while (true) {
        pico_toolset::Lsm6ds3Sample s;
        if (imu.read(s))
            std::printf("a %.2f %.2f %.2f g  g %.1f %.1f %.1f dps  steps %d\n", s.ax, s.ay, s.az,
                        s.gx, s.gy, s.gz, imu.step_count());
        sleep_ms(200);
    }
}
