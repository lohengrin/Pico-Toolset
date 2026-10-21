#pragma once

#include "pico_toolset/lsm6ds3.h"

namespace pico_toolset::configs::lsm6ds3 {

// The LSM6DS3 of Pimoroni's Multi-Sensor Stick on Presto's QW/ST connector
// (I2C0, GPIO40/41), default address 0x6A, 104 Hz. Validated on real hardware by examples/presto.
inline const Lsm6ds3Config kPimoroniPrestoMultiSensorStick = {
    .i2c = i2c0,
    .addr = 0x6A,
};

} // namespace pico_toolset::configs::lsm6ds3
