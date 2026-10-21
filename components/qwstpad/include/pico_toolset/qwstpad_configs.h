#pragma once

// Known-good QwstPadConfig presets. The pad hangs off a board's QW/ST I2C
// connector, so the preset names the bus; the caller still brings the bus up.

#include "pico_toolset/qwstpad.h"

namespace pico_toolset::configs::qwstpad {

// A QwSTPad (default address 0x21) on Pimoroni Presto's QW/ST connector,
// I2C0 on GPIO40 (SDA) / GPIO41 (SCL). Validated on real hardware by examples/presto.
inline const QwstPadConfig kPimoroniPresto = {
    .i2c = i2c0,
    .addr = 0x21,
};

} // namespace pico_toolset::configs::qwstpad
