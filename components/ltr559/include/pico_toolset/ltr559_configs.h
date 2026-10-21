#pragma once

#include "pico_toolset/ltr559.h"

namespace pico_toolset::configs::ltr559 {

// The LTR-559 of Pimoroni's Multi-Sensor Stick on Presto's QW/ST connector
// (I2C0, GPIO40/41), address 0x23. Validated on real hardware by examples/presto.
inline const Ltr559Config kPimoroniPrestoMultiSensorStick = {
    .i2c = i2c0,
    .addr = 0x23,
};

} // namespace pico_toolset::configs::ltr559
