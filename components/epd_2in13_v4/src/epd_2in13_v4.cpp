#include "pico_toolset/epd_2in13_v4.h"

#include <initializer_list>

#include "pico/stdlib.h"

namespace pico_toolset {

void Epd2in13V4::reset() {
    gpio_put(m_cfg.pin_rst, 1);
    sleep_ms(20);
    gpio_put(m_cfg.pin_rst, 0);
    sleep_ms(2);
    gpio_put(m_cfg.pin_rst, 1);
    sleep_ms(20);
}

void Epd2in13V4::send_command(uint8_t cmd) {
    gpio_put(m_cfg.pin_dc, 0);
    gpio_put(m_cfg.pin_cs, 0);
    spi_write_blocking(m_cfg.spi, &cmd, 1);
    gpio_put(m_cfg.pin_cs, 1);
}

void Epd2in13V4::send_data(uint8_t data) {
    gpio_put(m_cfg.pin_dc, 1);
    gpio_put(m_cfg.pin_cs, 0);
    spi_write_blocking(m_cfg.spi, &data, 1);
    gpio_put(m_cfg.pin_cs, 1);
}

void Epd2in13V4::wait_busy() {
    while (gpio_get(m_cfg.pin_busy) != 0) {
        sleep_ms(10);
    }
    sleep_ms(10);
}

void Epd2in13V4::set_windows(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end) {
    send_command(0x44);
    send_data((x_start >> 3) & 0xFF);
    send_data((x_end >> 3) & 0xFF);

    send_command(0x45);
    send_data(y_start & 0xFF);
    send_data((y_start >> 8) & 0xFF);
    send_data(y_end & 0xFF);
    send_data((y_end >> 8) & 0xFF);
}

void Epd2in13V4::set_cursor(uint16_t x_start, uint16_t y_start) {
    send_command(0x4E);
    send_data(x_start & 0xFF);

    send_command(0x4F);
    send_data(y_start & 0xFF);
    send_data((y_start >> 8) & 0xFF);
}

void Epd2in13V4::turn_on_display(uint8_t update_control) {
    send_command(0x22);
    send_data(update_control);
    send_command(0x20);
    wait_busy();
}

void Epd2in13V4::write_plane(uint8_t ram_command, const uint8_t* fb) {
    send_command(ram_command);
    for (int i = 0; i < kFramebufferSize; i++) {
        send_data(fb[i]);
    }
}

bool Epd2in13V4::init(const Epd2in13V4Config& config) {
    m_cfg = config;
    if (!m_cfg.spi) {
        return false;
    }

    for (uint8_t pin : {m_cfg.pin_rst, m_cfg.pin_dc, m_cfg.pin_cs}) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
    }
    gpio_init(m_cfg.pin_busy);
    gpio_set_dir(m_cfg.pin_busy, GPIO_IN);
    gpio_put(m_cfg.pin_cs, 1);

    spi_init(m_cfg.spi, m_cfg.spi_freq_hz);
    gpio_set_function(m_cfg.pin_sck, GPIO_FUNC_SPI);
    gpio_set_function(m_cfg.pin_mosi, GPIO_FUNC_SPI);

    m_full_refresh_next = true;
    return true;
}

void Epd2in13V4::init_panel() {
    reset();

    wait_busy();
    send_command(0x12);  // SWRESET
    wait_busy();

    send_command(0x01);  // driver output control
    send_data(0xF9);
    send_data(0x00);
    send_data(0x00);

    send_command(0x11);  // data entry mode
    send_data(0x03);

    set_windows(0, 0, kWidth - 1, kHeight - 1);
    set_cursor(0, 0);

    send_command(0x3C);  // border waveform
    send_data(0x05);

    send_command(0x21);  // display update control
    send_data(0x00);
    send_data(0x80);

    send_command(0x18);  // use the built-in temperature sensor
    send_data(0x80);
    wait_busy();
}

void Epd2in13V4::display(const uint8_t* fb) {
    write_plane(0x24, fb);
    turn_on_display(0xF7);
}

void Epd2in13V4::display_base(const uint8_t* fb) {
    write_plane(0x24, fb);
    write_plane(0x26, fb);
    turn_on_display(0xF7);
}

void Epd2in13V4::display_partial(const uint8_t* fb) {
    gpio_put(m_cfg.pin_rst, 0);
    sleep_ms(2);
    gpio_put(m_cfg.pin_rst, 1);

    send_command(0x3C);  // border waveform
    send_data(0x80);

    send_command(0x01);  // driver output control
    send_data(0xF9);
    send_data(0x00);
    send_data(0x00);

    send_command(0x11);  // data entry mode
    send_data(0x03);

    set_windows(0, 0, kWidth - 1, kHeight - 1);
    set_cursor(0, 0);

    write_plane(0x24, fb);
    turn_on_display(0xFF);
}

void Epd2in13V4::clear() {
    send_command(0x24);
    for (int i = 0; i < kFramebufferSize; i++) {
        send_data(0xFF);
    }
    turn_on_display(0xF7);
}

void Epd2in13V4::sleep() {
    send_command(0x10);  // deep sleep mode 1
    send_data(0x01);
    sleep_ms(100);
}

void Epd2in13V4::update(const uint8_t* fb) {
    if (m_full_refresh_next) {
        init_panel();
        display_base(fb);
        m_full_refresh_next = false;
    } else {
        display_partial(fb);
    }
    sleep();
}

void Epd2in13V4::clear_screen() {
    init_panel();
    clear();
    m_full_refresh_next = true;
}

} // namespace pico_toolset
