#pragma once

#include <cstdint>

#include "hardware/i2c.h"

namespace pico_toolset {

struct Ltr559Config {
    i2c_inst_t* i2c;      // already i2c_init()'d by the caller
    uint8_t     addr = 0x23;
};

struct Ltr559Reading {
    uint16_t proximity = 0; // 11-bit, higher = closer
    uint16_t als_ch0 = 0;   // visible + IR
    uint16_t als_ch1 = 0;   // IR
    float    lux = 0.0f;
};

// LTR-559 ambient light + proximity sensor with the defaults Pimoroni's
// driver uses: ALS gain 4x, 50 ms integration/repeat, proximity LED 50 mA
// 100% duty 30 Hz 1 pulse, 100 ms proximity rate. Lux uses the datasheet's
// ratio-dependent coefficients. Ported from Pimoroni's ltr559 driver (MIT).
class Ltr559 {
public:
    // False if the part id does not match an LTR-559 (0x09) or the bus is dead.
    bool init(const Ltr559Config& config);

    // Updates `reading` with whatever new proximity/ALS data is ready and
    // returns true if anything changed.
    bool poll(Ltr559Reading& reading);

    [[nodiscard]] uint8_t part_id();
    [[nodiscard]] uint8_t manufacturer_id();

private:
    bool read_regs(uint8_t reg, uint8_t* buf, size_t len);
    bool write_reg(uint8_t reg, uint8_t value);

    Ltr559Config m_cfg{};
};

} // namespace pico_toolset
