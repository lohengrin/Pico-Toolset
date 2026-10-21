#pragma once

#include "pico_toolset/bme280.h"

namespace pico_toolset::configs::bme280 {

// The BME280 of Pimoroni's Multi-Sensor Stick on Presto's QW/ST connector
// (I2C0, GPIO40/41), default address 0x76. Validated on real hardware by examples/presto.
inline const Bme280Config kPimoroniPrestoMultiSensorStick = {
    .i2c = i2c0,
    .addr = 0x76,
};

} // namespace pico_toolset::configs::bme280
