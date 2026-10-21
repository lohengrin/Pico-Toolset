#include "pico_toolset/bme280.h"

#include "bosch/bme280.h"
#include "pico/stdlib.h"

namespace pico_toolset {

namespace {

constexpr uint32_t kTimeoutUs = 20'000;

struct Bus {
    i2c_inst_t* i2c;
    uint8_t addr;
};

BME280_INTF_RET_TYPE bus_read(uint8_t reg, uint8_t* data, uint32_t len, void* intf) {
    auto* b = static_cast<Bus*>(intf);
    if (i2c_write_timeout_us(b->i2c, b->addr, &reg, 1, true, kTimeoutUs) != 1) return BME280_E_COMM_FAIL;
    if (i2c_read_timeout_us(b->i2c, b->addr, data, len, false, kTimeoutUs) != static_cast<int>(len))
        return BME280_E_COMM_FAIL;
    return BME280_OK;
}

BME280_INTF_RET_TYPE bus_write(uint8_t reg, const uint8_t* data, uint32_t len, void* intf) {
    auto* b = static_cast<Bus*>(intf);
    uint8_t buf[33];
    if (len > sizeof(buf) - 1) return BME280_E_COMM_FAIL;
    buf[0] = reg;
    for (uint32_t i = 0; i < len; ++i) buf[i + 1] = data[i];
    return i2c_write_timeout_us(b->i2c, b->addr, buf, len + 1, false, kTimeoutUs) == static_cast<int>(len + 1)
        ? BME280_OK : BME280_E_COMM_FAIL;
}

void bus_delay(uint32_t us, void*) { sleep_us(us); }

Bme280Reading convert(int8_t rc, const bme280_data& d) {
    Bme280Reading r;
    r.valid = rc == BME280_OK;
    r.temperature_c = static_cast<float>(d.temperature);
    r.pressure_pa = static_cast<float>(d.pressure);
    r.humidity_pct = static_cast<float>(d.humidity);
    return r;
}

} // namespace

struct Bme280::Impl {
    bme280_dev dev{};
    bme280_settings settings{};
    Bus bus{};
    bool ready = false;
};

Bme280::Bme280() : m_impl(new Impl) {}
Bme280::~Bme280() { delete m_impl; }

bool Bme280::init(const Bme280Config& config) {
    auto& d = m_impl->dev;
    m_impl->bus = {config.i2c, config.addr};
    d.intf_ptr = &m_impl->bus;
    d.intf = BME280_I2C_INTF;
    d.read = bus_read;
    d.write = bus_write;
    d.delay_us = bus_delay;

    if (bme280_init(&d) != BME280_OK) return false;

    auto& st = m_impl->settings;
    st.filter = BME280_FILTER_COEFF_2;
    st.standby_time = BME280_STANDBY_TIME_0_5_MS;
    st.osr_p = BME280_OVERSAMPLING_16X;
    st.osr_t = BME280_OVERSAMPLING_2X;
    st.osr_h = BME280_OVERSAMPLING_1X;
    if (bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &st, &d) != BME280_OK) return false;
    if (bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &d) != BME280_OK) return false;
    m_impl->ready = true;
    return true;
}

Bme280Reading Bme280::read() {
    bme280_data d{};
    if (!m_impl->ready) return {};
    return convert(bme280_get_sensor_data(BME280_ALL, &d, &m_impl->dev), d);
}

Bme280Reading Bme280::read_forced() {
    bme280_data d{};
    if (!m_impl->ready) return {};
    auto& dev = m_impl->dev;
    if (bme280_set_sensor_mode(BME280_POWERMODE_FORCED, &dev) != BME280_OK) return {};
    uint32_t delay_us = 0;
    bme280_cal_meas_delay(&delay_us, &m_impl->settings);
    dev.delay_us(delay_us, dev.intf_ptr);
    return convert(bme280_get_sensor_data(BME280_ALL, &d, &dev), d);
}

} // namespace pico_toolset
