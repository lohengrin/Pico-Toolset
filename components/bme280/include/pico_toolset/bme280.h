#pragma once

#include <cstdint>

#include "hardware/i2c.h"

namespace pico_toolset {

struct Bme280Config {
    i2c_inst_t* i2c;      // already i2c_init()'d by the caller
    uint8_t     addr;     // 0x76 (SDO low) or 0x77 (SDO high)
};

struct Bme280Reading {
    float temperature_c = 0.0f;
    float pressure_pa = 0.0f;
    float humidity_pct = 0.0f;
    bool  valid = false;
};

// BME280 temperature / pressure / humidity sensor on top of Bosch Sensortec's
// reference driver (BSD-3-Clause, vendored under src/bosch/, float build).
// Defaults match Pimoroni's driver: IIR filter 2, 0.5 ms standby, pressure
// x16 / temperature x2 / humidity x1 oversampling, normal mode.
class Bme280 {
public:
    Bme280();
    ~Bme280();
    Bme280(const Bme280&) = delete;
    Bme280& operator=(const Bme280&) = delete;

    // False if the chip does not answer or reports a wrong id.
    bool init(const Bme280Config& config);

    // Latest conversion (normal mode, free-running).
    Bme280Reading read();
    // Triggers one conversion and waits for it (use after switching to forced
    // mode or to save power); blocks for the measurement time.
    Bme280Reading read_forced();

private:
    struct Impl;
    Impl* m_impl;
};

} // namespace pico_toolset
