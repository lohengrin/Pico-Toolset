#pragma once

#include <cstdint>

namespace pico_toolset {

// One touch reading: pressed state + raw (uncalibrated) coordinates. Mapping
// these into pixel space is driver-agnostic -- see e.g. xpt2046_calibration.h.
struct TouchSample {
    bool     pressed = false;
    uint16_t raw_x   = 0;
    uint16_t raw_y   = 0;
};

// Low-level contract shared by touch controllers that report raw (x, y)
// samples off a shared bus (e.g. Xpt2046Touch).
//
// SHARED-BUS CONTRACT: an implementer does not call spi_init()/i2c_init()
// itself -- it shares a bus a display driver (or the caller directly)
// already brought up, taking over only its own CS/addressing for the
// duration of each read(). Callers must not call read() while another
// device on the same bus has an open transfer.
class TouchPanel {
public:
    virtual ~TouchPanel() = default;

    // Polls for a touch and returns the latest sample.
    [[nodiscard]] virtual TouchSample read() = 0;
};

} // namespace pico_toolset
