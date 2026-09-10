#include "pico_toolset/ssd1306.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "font.h"

namespace pico_toolset {

void Ssd1306::swap(int32_t* a, int32_t* b) {
    int32_t t = *a;
    *a = *b;
    *b = t;
}

void Ssd1306::fancy_write(i2c_inst_t* i2c, uint8_t addr, const uint8_t* src, size_t len, const char* name) {
    switch (i2c_write_blocking(i2c, addr, src, len, false)) {
    case PICO_ERROR_GENERIC:
        printf("[%s] addr not acknowledged!\n", name);
        break;
    case PICO_ERROR_TIMEOUT:
        printf("[%s] timeout!\n", name);
        break;
    default:
        break;
    }
}

inline void Ssd1306::write(uint8_t val) {
    uint8_t d[2] = {0x00, val};
    fancy_write(m_i2c_i, m_address, d, 2, "ssd1306_write");
}

bool Ssd1306::init(const Ssd1306Config& config) {
    m_width       = config.width;
    m_height      = config.height;
    m_pages       = m_height / 8;
    m_address     = config.address;
    m_i2c_i       = config.i2c_instance;
    m_external_vcc = config.external_vcc;

    i2c_init(m_i2c_i, config.i2c_freq_hz);
    gpio_set_function(config.sda_pin, GPIO_FUNC_I2C);
    gpio_set_function(config.scl_pin, GPIO_FUNC_I2C);
    gpio_pull_up(config.sda_pin);
    gpio_pull_up(config.scl_pin);

    m_bufsize = m_pages * m_width;
    if ((m_buffer = static_cast<uint8_t*>(malloc(m_bufsize + 1))) == nullptr) {
        m_bufsize = 0;
        return false;
    }
    ++m_buffer;

    uint8_t cmds[] = {
        kSetDisp,
        kSetDispClkDiv, 0x80,
        kSetMuxRatio, static_cast<uint8_t>(m_height - 1),
        kSetDispOffset, 0x00,
        kSetDispStartLine,
        kSetChargePump, static_cast<uint8_t>(m_external_vcc ? 0x10 : 0x14),
        kSetSegRemap | 0x01,
        kSetComOutDir | 0x08,
        kSetComPinCfg, static_cast<uint8_t>(m_width > 2 * m_height ? 0x02 : 0x12),
        kSetContrast, 0xff,
        kSetPrecharge, static_cast<uint8_t>(m_external_vcc ? 0x22 : 0xF1),
        kSetVcomDesel, 0x30,
        kSetEntireOn,
        kSetNormInv,
        kSetDisp | 0x01,
        kSetMemAddr, 0x00,
    };

    for (size_t i = 0; i < sizeof(cmds); ++i)
        write(cmds[i]);

    return true;
}

void Ssd1306::deinit() {
    if (m_buffer)
        free(m_buffer - 1);
    m_buffer = nullptr;
    m_bufsize = 0;
}

void Ssd1306::power_off() {
    write(kSetDisp | 0x00);
}

void Ssd1306::power_on() {
    write(kSetDisp | 0x01);
}

void Ssd1306::contrast(uint8_t val) {
    write(kSetContrast);
    write(val);
}

void Ssd1306::invert(uint8_t inv) {
    write(kSetNormInv | (inv & 1));
}

void Ssd1306::clear() {
    memset(m_buffer, 0, m_bufsize);
}

void Ssd1306::draw_pixel(uint32_t x, uint32_t y) {
    if (x >= m_width || y >= m_height || !m_buffer)
        return;
    m_buffer[x + m_width * (y >> 3)] |= static_cast<uint8_t>(0x1 << (y & 0x07));
}

void Ssd1306::draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    if (x1 > x2) {
        swap(&x1, &x2);
        swap(&y1, &y2);
    }

    if (x1 == x2) {
        if (y1 > y2)
            swap(&y1, &y2);
        for (int32_t i = y1; i <= y2; ++i)
            draw_pixel(x1, i);
        return;
    }

    float m = static_cast<float>(y2 - y1) / static_cast<float>(x2 - x1);
    for (int32_t i = x1; i <= x2; ++i) {
        float y = m * static_cast<float>(i - x1) + static_cast<float>(y1);
        draw_pixel(i, static_cast<uint32_t>(y));
    }
}

void Ssd1306::draw_square(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    if (!m_buffer)
        return;
    for (uint32_t i = 0; i < width; ++i)
        for (uint32_t j = 0; j < height; ++j)
            draw_pixel(x + i, y + j);
}

void Ssd1306::draw_empty_square(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    draw_line(x, y, static_cast<int32_t>(x + width), static_cast<int32_t>(y));
    draw_line(x, static_cast<int32_t>(y + height), static_cast<int32_t>(x + width), static_cast<int32_t>(y + height));
    draw_line(x, static_cast<int32_t>(y), x, static_cast<int32_t>(y + height));
    draw_line(static_cast<int32_t>(x + width), static_cast<int32_t>(y),
              static_cast<int32_t>(x + width), static_cast<int32_t>(y + height));
}

void Ssd1306::draw_char_with_font(uint32_t x, uint32_t y, uint32_t scale,
                                  const uint8_t* font, char c) {
    if (!m_buffer)
        return;
    if (c < font[3] || c > font[4])
        return;

    uint32_t parts_per_line = (font[0] >> 3) + ((font[0] & 7) > 0);
    for (uint8_t w = 0; w < font[1]; ++w) {
        uint32_t pp = (c - font[3]) * font[1] * parts_per_line + w * parts_per_line + 5;
        for (uint32_t lp = 0; lp < parts_per_line; ++lp) {
            uint8_t line = font[pp];
            for (int8_t j = 0; j < 8; ++j, line >>= 1) {
                if (line & 1)
                    draw_square(x + w * scale, y + ((lp << 3) + j) * scale, scale, scale);
            }
            ++pp;
        }
    }
}

void Ssd1306::draw_string_with_font(uint32_t x, uint32_t y, uint32_t scale,
                                    const uint8_t* font, const char* s) {
    for (int32_t x_n = static_cast<int32_t>(x); *s; x_n += (font[1] + font[2]) * scale) {
        draw_char_with_font(x_n, y, scale, font, *(s++));
    }
}

void Ssd1306::draw_char(uint32_t x, uint32_t y, uint32_t scale, char c) {
    draw_char_with_font(x, y, scale, font_8x5, c);
}

void Ssd1306::draw_string(uint32_t x, uint32_t y, uint32_t scale, const char* s) {
    draw_string_with_font(x, y, scale, font_8x5, s);
}

inline uint32_t Ssd1306::bmp_get_val(const uint8_t* data, size_t offset, uint8_t size) {
    switch (size) {
    case 1: return data[offset];
    case 2: return data[offset] | (data[offset + 1] << 8);
    case 4: return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
    default: __builtin_unreachable();
    }
    __builtin_unreachable();
}

void Ssd1306::bmp_show_image_with_offset(const uint8_t* data, long size,
                                         uint32_t x_offset, uint32_t y_offset) {
    if (size < 54)
        return;

    const uint32_t bfOffBits   = bmp_get_val(data, 10, 4);
    const uint32_t biSize      = bmp_get_val(data, 14, 4);
    const int32_t  biWidth     = static_cast<int32_t>(bmp_get_val(data, 18, 4));
    const int32_t  biHeight    = static_cast<int32_t>(bmp_get_val(data, 22, 4));
    const uint16_t biBitCount  = static_cast<uint16_t>(bmp_get_val(data, 28, 2));
    const uint32_t biCompression = bmp_get_val(data, 30, 4);

    if (biBitCount != 1 || biCompression != 0)
        return;

    const int table_start = 14 + biSize;
    uint8_t color_val = 0;
    for (uint8_t i = 0; i < 2; ++i) {
        if (!((data[table_start + i * 4] << 16) | (data[table_start + i * 4 + 1] << 8) |
              data[table_start + i * 4 + 2])) {
            color_val = i;
            break;
        }
    }

    uint32_t bytes_per_line = static_cast<uint32_t>(biWidth / 8) + (biWidth & 7 ? 1 : 0);
    if (bytes_per_line & 3)
        bytes_per_line = (bytes_per_line ^ (bytes_per_line & 3)) + 4;

    const uint8_t* img_data = data + bfOffBits;

    int step = biHeight > 0 ? -1 : 1;
    int border = biHeight > 0 ? -1 : biHeight;
    for (uint32_t y = biHeight > 0 ? static_cast<uint32_t>(biHeight - 1) : 0; y != border; y += step) {
        for (uint32_t x = 0; x < static_cast<uint32_t>(biWidth); ++x) {
            if (((img_data[x >> 3] >> (7 - (x & 7))) & 1) == color_val)
                draw_pixel(x_offset + x, y_offset + y);
        }
        img_data += bytes_per_line;
    }
}

void Ssd1306::bmp_show_image(const uint8_t* data, long size) {
    bmp_show_image_with_offset(data, size, 0, 0);
}

void Ssd1306::show() {
    if (!m_buffer)
        return;
    uint8_t payload[] = {
        kSetColAddr, 0, static_cast<uint8_t>(m_width - 1),
        kSetPageAddr, 0, static_cast<uint8_t>(m_pages - 1)
    };
    if (m_width == 64) {
        payload[1] += 32;
        payload[2] += 32;
    }

    for (size_t i = 0; i < sizeof(payload); ++i)
        write(payload[i]);

    *(m_buffer - 1) = 0x40;
    fancy_write(m_i2c_i, m_address, m_buffer - 1, m_bufsize + 1, "ssd1306_show");
}

} // namespace pico_toolset