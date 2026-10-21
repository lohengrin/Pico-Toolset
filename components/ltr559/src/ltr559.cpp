// LTR-559 driver, ported from Pimoroni's ltr559 driver (MIT).
#include "pico_toolset/ltr559.h"

#include "pico/stdlib.h"

namespace pico_toolset {

namespace {

constexpr uint8_t ALS_CONTROL = 0x80, PS_CONTROL = 0x81, PS_LED = 0x82, PS_N_PULSES = 0x83,
                  PS_MEAS_RATE = 0x84, ALS_MEAS_RATE = 0x85, PART_ID = 0x86,
                  MANUFACTURER_ID = 0x87, ALS_DATA_CH1 = 0x88, ALS_PS_STATUS = 0x8C,
                  PS_DATA = 0x8D, INTERRUPT = 0x8F, PS_THRESHOLD_UPPER = 0x90,
                  PS_THRESHOLD_LOWER = 0x92, PS_OFFSET = 0x94, ALS_THRESHOLD_UPPER = 0x97,
                  ALS_THRESHOLD_LOWER = 0x99;

constexpr uint8_t kValidPartId = 0x09;
constexpr uint32_t kTimeoutUs = 5000;

constexpr uint16_t kGain[8] = {1, 2, 4, 8, 0, 0, 48, 96};
constexpr int kCh0[4] = {17743, 42785, 5926, 0};
constexpr int kCh1[4] = {-11059, 19548, -1185, 0};

// Configured below (values from Pimoroni's defaults).
constexpr uint8_t kGain4xIndex = 2;
constexpr uint16_t kIntegrationMs = 50;

} // namespace

bool Ltr559::read_regs(uint8_t reg, uint8_t* buf, size_t len) {
    if (i2c_write_timeout_us(m_cfg.i2c, m_cfg.addr, &reg, 1, true, kTimeoutUs) != 1) return false;
    return i2c_read_timeout_us(m_cfg.i2c, m_cfg.addr, buf, len, false, kTimeoutUs) == static_cast<int>(len);
}

bool Ltr559::write_reg(uint8_t reg, uint8_t value) {
    const uint8_t buf[2] = {reg, value};
    return i2c_write_timeout_us(m_cfg.i2c, m_cfg.addr, buf, 2, false, kTimeoutUs) == 2;
}

uint8_t Ltr559::part_id() {
    uint8_t v = 0;
    read_regs(PART_ID, &v, 1);
    return (v >> 4) & 0x0F;
}

uint8_t Ltr559::manufacturer_id() {
    uint8_t v = 0;
    read_regs(MANUFACTURER_ID, &v, 1);
    return v;
}

bool Ltr559::init(const Ltr559Config& config) {
    m_cfg = config;
    if (part_id() != kValidPartId) return false;

    write_reg(ALS_CONTROL, 1 << 1);            // software reset
    for (int i = 0; i < 20; ++i) {
        uint8_t v = 0;
        if (read_regs(ALS_CONTROL, &v, 1) && !(v & (1 << 1))) break;
        sleep_ms(10);
    }

    write_reg(INTERRUPT, (1 << 2) | 0b11);     // active-high INT, ALS + PS interrupts
    write_reg(PS_LED, 0b011'11'100);           // 30 Hz pulses, 100% duty, 50 mA
    write_reg(PS_N_PULSES, 1);
    write_reg(ALS_CONTROL, (kGain4xIndex << 2) | 1);   // active, 4x gain
    write_reg(PS_CONTROL, (1 << 5) | 0b11);    // active, saturation indicator
    write_reg(PS_MEAS_RATE, 3);                // 100 ms
    write_reg(ALS_MEAS_RATE, (1 << 3) | 0);    // 50 ms integration, 50 ms repeat

    const uint8_t als_thr[4] = {0x00, 0x00, 0xFF, 0xFF}; // lower 0x0000, upper 0xFFFF (big-endian pairs)
    write_reg(ALS_THRESHOLD_LOWER, als_thr[0]);
    write_reg(ALS_THRESHOLD_LOWER + 1, als_thr[1]);
    write_reg(ALS_THRESHOLD_UPPER, als_thr[2]);
    write_reg(ALS_THRESHOLD_UPPER + 1, als_thr[3]);
    write_reg(PS_THRESHOLD_LOWER, 0xFF); write_reg(PS_THRESHOLD_LOWER + 1, 0x07);
    write_reg(PS_THRESHOLD_UPPER, 0xFF); write_reg(PS_THRESHOLD_UPPER + 1, 0x07);
    write_reg(PS_OFFSET, 0);
    return true;
}

bool Ltr559::poll(Ltr559Reading& r) {
    uint8_t status;
    if (!read_regs(ALS_PS_STATUS, &status, 1)) return false;
    const bool als_new = (status >> 3) & 1 || (status >> 2) & 1;
    const bool ps_new = (status >> 1) & 1 || status & 1;
    bool updated = false;

    if (ps_new) {
        uint8_t d[2];
        if (read_regs(PS_DATA, d, 2)) {
            r.proximity = static_cast<uint16_t>((d[0] | (d[1] << 8)) & 0x07FF);
            updated = true;
        }
    }
    if (als_new) {
        uint8_t d[4];
        if (read_regs(ALS_DATA_CH1, d, 4)) {
            r.als_ch1 = static_cast<uint16_t>(d[0] | (d[1] << 8));
            r.als_ch0 = static_cast<uint16_t>(d[2] | (d[3] << 8));
            const uint16_t gain = kGain[(status >> 4) & 0b111];

            float ratio = 101.0f;
            const uint32_t sum = static_cast<uint32_t>(r.als_ch0) + r.als_ch1;
            if (sum > 0) ratio = r.als_ch1 * 100.0f / static_cast<float>(sum);
            const int idx = ratio < 45 ? 0 : ratio < 64 ? 1 : ratio < 85 ? 2 : 3;

            float lux = static_cast<float>(static_cast<int32_t>(r.als_ch0) * kCh0[idx] -
                                           static_cast<int32_t>(r.als_ch1) * kCh1[idx]);
            lux /= kIntegrationMs / 100.0f;
            if (gain) lux /= gain;
            r.lux = lux / 10000.0f;
            updated = true;
        }
    }
    return updated;
}

} // namespace pico_toolset
