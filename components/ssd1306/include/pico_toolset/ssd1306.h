#pragma once

#include <cstdint>
#include <cstddef>

#include "hardware/i2c.h"
#include "pico/stdlib.h"

namespace pico_toolset {

// Configuration for the SSD1306 OLED display. All pins and timings are
// configurable so the driver adapts to any board wiring.
//
// i2c_instance/sda_pin/scl_pin have no working default -- they're board
// wiring, not driver behavior. No validated per-board preset exists yet for
// this component (unlike e.g. pico_toolset::Ili9486Config's
// ili9486_configs.h) -- fill them in for your own board, or contribute a
// preset once you've validated one on real hardware.
struct Ssd1306Config {
    i2c_inst_t* i2c_instance = nullptr; // I2C peripheral to use -- must be set
    uint8_t     sda_pin;                // SDA GPIO -- must be set
    uint8_t     scl_pin;                // SCL GPIO -- must be set
    uint32_t    i2c_freq_hz  = 400000; // I2C clock speed
    uint8_t     address      = 0x3C;  // I2C slave address
    uint8_t     width        = 128;   // Pixel width
    uint8_t     height       = 64;    // Pixel height
    bool        external_vcc = false; // False = internal charge pump
};

// Driver for SSD1306 monochrome OLED displays over I2C (128x64, 128x32,
// 64x48...). Uses an in-memory framebuffer; call show() to flush it.
//
// Derived from David Schramm's rpi-pico-ssd1306 driver (MIT) with a
// config-struct init and added framebuffer accessors.
class Ssd1306 {
public:
    // Initialize the I2C bus and the display. Must be called once before any
    // drawing. Returns false on framebuffer allocation failure.
    bool init(const Ssd1306Config& config);

    // Free the framebuffer.
    void deinit();

    // Flush the in-memory buffer to the display.
    void show();

    // Clear the in-memory buffer.
    void clear();

    void power_on();
    void power_off();
    void contrast(uint8_t val);
    void invert(uint8_t inv);

    // Drawing primitives (write into the buffer).
    void draw_pixel(uint32_t x, uint32_t y);
    void draw_line(int32_t x1, int32_t y1, int32_t x2, int32_t y2);
    void draw_square(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void draw_empty_square(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void draw_char(uint32_t x, uint32_t y, uint32_t scale, char c);
    void draw_string(uint32_t x, uint32_t y, uint32_t scale, const char* s);
    // Font-scoped variants (raw embedded font pointer).
    void draw_char_with_font(uint32_t x, uint32_t y, uint32_t scale, const uint8_t* font, char c);
    void draw_string_with_font(uint32_t x, uint32_t y, uint32_t scale, const uint8_t* font, const char* s);
    void bmp_show_image(const uint8_t* data, long size);
    void bmp_show_image_with_offset(const uint8_t* data, long size, uint32_t x_offset, uint32_t y_offset);

    // Accessors (used by the screen-backend integration).
    uint8_t  width() const { return m_width; }
    uint8_t  height() const { return m_height; }
    uint8_t* buffer() { return m_buffer; }
    size_t   buffer_size() const { return m_bufsize; }

private:
    enum Command : uint8_t {
        kSetContrast = 0x81,
        kSetEntireOn = 0xA4,
        kSetNormInv  = 0xA6,
        kSetDisp     = 0xAE,
        kSetMemAddr  = 0x20,
        kSetColAddr  = 0x21,
        kSetPageAddr = 0x22,
        kSetDispStartLine = 0x40,
        kSetSegRemap = 0xA0,
        kSetMuxRatio = 0xA8,
        kSetComOutDir = 0xC0,
        kSetDispOffset = 0xD3,
        kSetComPinCfg = 0xDA,
        kSetDispClkDiv = 0xD5,
        kSetPrecharge = 0xD9,
        kSetVcomDesel = 0xDB,
        kSetChargePump = 0x8D
    };

    inline void write(uint8_t val);
    inline static void swap(int32_t* a, int32_t* b);
    inline static void fancy_write(i2c_inst_t* i2c, uint8_t addr,
                                   const uint8_t* src, size_t len, const char* name);
    inline static uint32_t bmp_get_val(const uint8_t* data, size_t offset, uint8_t size);

    uint8_t     m_width   = 0;
    uint8_t     m_height  = 0;
    uint8_t     m_pages   = 0;
    uint8_t     m_address = 0;
    i2c_inst_t* m_i2c_i   = nullptr;
    uint8_t*    m_buffer  = nullptr;
    size_t      m_bufsize = 0;
    bool        m_external_vcc = false;
};

} // namespace pico_toolset