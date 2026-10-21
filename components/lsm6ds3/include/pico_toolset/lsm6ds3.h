#pragma once

#include <cstdint>

#include "hardware/i2c.h"

namespace pico_toolset {

struct Lsm6ds3Config {
    i2c_inst_t* i2c;      // already i2c_init()'d by the caller
    uint8_t     addr;     // 0x6A (SDO low) or 0x6B (SDO high)
    uint8_t     odr_mode = 0x40; // CTRL1_XL/CTRL2_G value: 0x40 = 104 Hz normal, 0x50 = 208 Hz, 0x60 = 416 Hz, 0x02 = 26 Hz low power
};

struct Lsm6ds3Raw {
    int16_t ax, ay, az; // LSB, +-2 g full scale
    int16_t gx, gy, gz; // LSB, +-250 dps full scale
};

struct Lsm6ds3Sample {
    float ax, ay, az; // g
    float gx, gy, gz; // degrees per second
};

// LSM6DS3 6-axis IMU: accelerometer + gyroscope, pedometer and the tap /
// tilt / significant-motion / free-fall events. Fixed at +-2 g and +-250 dps.
// Ported from Pimoroni's Presto lsm6ds3.py (MIT).
class Lsm6ds3 {
public:
    // False if the device does not answer. who_am_i() is recorded but not
    // enforced (0x69 on the LSM6DS3, 0x6A on the TR-C variant).
    bool init(const Lsm6ds3Config& config);
    [[nodiscard]] uint8_t who_am_i() const { return m_who; }

    bool read_raw(Lsm6ds3Raw& out);
    bool read(Lsm6ds3Sample& out);

    [[nodiscard]] int16_t step_count();
    void reset_step_count();

    [[nodiscard]] bool tilt_detected();
    [[nodiscard]] bool significant_motion_detected();
    [[nodiscard]] bool single_tap_detected();
    [[nodiscard]] bool double_tap_detected();
    [[nodiscard]] bool free_fall_detected();

private:
    bool read_regs(uint8_t reg, uint8_t* buf, size_t len);
    bool write_reg(uint8_t reg, uint8_t value);
    uint8_t read_reg(uint8_t reg);

    Lsm6ds3Config m_cfg{};
    uint8_t m_who = 0;
};

} // namespace pico_toolset
