#pragma once

#include <cstdint>

#include "hardware/i2c.h"
#include "pico_toolset/touch_panel.h"

namespace pico_toolset {

// All fields are board wiring -- no defaults; see ft6236_configs.h.
struct Ft6236Config {
    i2c_inst_t* i2c;
    uint8_t  pin_sda;
    uint8_t  pin_scl;
    uint8_t  pin_int;       // active-low "touch data ready"; 0xFF = poll the bus every read()
    uint8_t  addr;          // 7-bit address (0x38 on most FT6x36 boards, 0x48 on Presto)
    uint16_t width;         // output coordinate space (the controller reports 0..479)
    uint16_t height;
    bool     rotate_180;    // follow a rotated display
    uint32_t i2c_freq_hz = 400'000;
};

// FT6236 capacitive touch controller, up to two touch points. Implements
// TouchPanel; read() returns touch 1 in panel pixel coordinates (raw_x/raw_y
// are already scaled, no calibration needed). Unlike the SPI-bus TouchPanel
// implementers this one owns its own I2C bus and brings it up in init().
//
// Ported from Pimoroni's Presto touch.py (MIT): poll when INT is low or a
// touch is held, back off after repeated NACKs, unwedge SDA by clocking SCL.
class Ft6236 : public TouchPanel {
public:
    void init(const Ft6236Config& config);

    // Latest touch 1. Returns the previous state unchanged while backing off
    // from bus errors.
    [[nodiscard]] TouchSample read() override;

    // Second simultaneous touch from the most recent read().
    [[nodiscard]] TouchSample touch2() const { return m_t2; }

    // True if the last transaction succeeded (false while in error back-off).
    [[nodiscard]] bool bus_ok() const { return m_errors == 0; }

private:
    bool transfer();
    void recover_bus();
    void decode(const uint8_t* d, TouchSample& out) const;

    Ft6236Config m_cfg{};
    TouchSample m_t1{};
    TouchSample m_t2{};
    uint8_t m_errors = 0;
    uint8_t m_skip = 0;
};

} // namespace pico_toolset
