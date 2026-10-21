// LSM6DS3 driver, ported from Pimoroni's Presto lsm6ds3.py (MIT).
#include "pico_toolset/lsm6ds3.h"

namespace pico_toolset {

namespace {

constexpr uint8_t WHO_AM_I = 0x0F, CTRL1_XL = 0x10, CTRL2_G = 0x11, CTRL10_C = 0x19;
constexpr uint8_t OUTX_L_G = 0x22, STEP_COUNTER_L = 0x4B, TAP_SRC = 0x1C, TAP_CFG = 0x58;
constexpr uint8_t FUNC_SRC1 = 0x53, TAP_THS_6D = 0x59, WAKE_UP_THS = 0x5B, WAKE_UP_SRC = 0x1B;
constexpr uint8_t INT_DUR2 = 0x5A;

constexpr uint8_t kSetFuncEn = 0xBD, kResetSteps = 0x02, kTapEnXyz = 0x8E;
constexpr uint8_t kTapThreshold = 0x02, kDoubleTapEn = 0x80, kDoubleTapDur = 0x20;

constexpr uint32_t kTimeoutUs = 5000;
constexpr float kAccelG = 0.061f / 1000.0f;     // g per LSB at +-2 g
constexpr float kGyroDps = 8.75f / 1000.0f;     // dps per LSB at +-250 dps

} // namespace

bool Lsm6ds3::read_regs(uint8_t reg, uint8_t* buf, size_t len) {
    if (i2c_write_timeout_us(m_cfg.i2c, m_cfg.addr, &reg, 1, true, kTimeoutUs) != 1) return false;
    return i2c_read_timeout_us(m_cfg.i2c, m_cfg.addr, buf, len, false, kTimeoutUs) == static_cast<int>(len);
}

bool Lsm6ds3::write_reg(uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return i2c_write_timeout_us(m_cfg.i2c, m_cfg.addr, buf, 2, false, kTimeoutUs) == 2;
}

uint8_t Lsm6ds3::read_reg(uint8_t reg) {
    uint8_t v = 0;
    read_regs(reg, &v, 1);
    return v;
}

bool Lsm6ds3::init(const Lsm6ds3Config& config) {
    m_cfg = config;
    if (!read_regs(WHO_AM_I, &m_who, 1)) return false;

    write_reg(CTRL2_G, m_cfg.odr_mode);      // gyro on
    write_reg(CTRL1_XL, m_cfg.odr_mode);     // accel on
    write_reg(CTRL10_C, kResetSteps);        // clear the pedometer
    write_reg(CTRL10_C, kSetFuncEn);         // tap, tilt, significant motion
    write_reg(TAP_CFG, kTapEnXyz);
    write_reg(WAKE_UP_THS, kDoubleTapEn);
    write_reg(TAP_THS_6D, kTapThreshold);
    write_reg(INT_DUR2, kDoubleTapDur);      // double-tap max gap
    return true;
}

bool Lsm6ds3::read_raw(Lsm6ds3Raw& out) {
    uint8_t d[12];
    if (!read_regs(OUTX_L_G, d, sizeof(d))) return false;
    auto s16 = [&](int i) { return static_cast<int16_t>(d[i] | (d[i + 1] << 8)); };
    out = {s16(6), s16(8), s16(10), s16(0), s16(2), s16(4)};
    return true;
}

bool Lsm6ds3::read(Lsm6ds3Sample& out) {
    Lsm6ds3Raw r;
    if (!read_raw(r)) return false;
    out = {r.ax * kAccelG, r.ay * kAccelG, r.az * kAccelG,
           r.gx * kGyroDps, r.gy * kGyroDps, r.gz * kGyroDps};
    return true;
}

int16_t Lsm6ds3::step_count() {
    uint8_t d[2] = {0, 0};
    read_regs(STEP_COUNTER_L, d, 2);
    return static_cast<int16_t>(d[0] | (d[1] << 8));
}

void Lsm6ds3::reset_step_count() {
    write_reg(CTRL10_C, kResetSteps);
    write_reg(CTRL10_C, kSetFuncEn);
}

bool Lsm6ds3::tilt_detected() { return (read_reg(FUNC_SRC1) >> 5) & 1; }
bool Lsm6ds3::significant_motion_detected() { return (read_reg(FUNC_SRC1) >> 6) & 1; }
bool Lsm6ds3::single_tap_detected() { return (read_reg(TAP_SRC) >> 5) & 1; }
bool Lsm6ds3::double_tap_detected() { return (read_reg(TAP_SRC) >> 4) & 1; }
bool Lsm6ds3::free_fall_detected() { return (read_reg(WAKE_UP_SRC) >> 5) & 1; }

} // namespace pico_toolset
