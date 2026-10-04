#pragma once

#include <cstdint>

#include "hardware/spi.h"

namespace pico_toolset {

struct Epd2in13V4Config {
    spi_inst_t* spi = nullptr;
    uint8_t pin_sck{};
    uint8_t pin_mosi{};
    uint8_t pin_cs{};
    uint8_t pin_dc{};
    uint8_t pin_rst{};
    uint8_t pin_busy{};
    uint32_t spi_freq_hz = 4'000'000;
};

// Waveshare 2.13" V4 e-paper (SSD1680-class, 122x250, 1 bit per pixel, MSB
// leftmost, 1 = white). Framebuffers passed in are kBytesPerRow * kHeight bytes,
// in the panel's native portrait orientation.
//
// Waveshare's reference driver (MIT, see components/epd_2in13_v4/LICENSE) ported
// from C globals to a config-driven class.
class Epd2in13V4 {
public:
    static constexpr int kWidth = 122;
    static constexpr int kHeight = 250;
    static constexpr int kBytesPerRow = (kWidth + 7) / 8;
    static constexpr int kFramebufferSize = kBytesPerRow * kHeight;

    // Claims the SPI/GPIO resources. Does not talk to the panel.
    bool init(const Epd2in13V4Config& config);

    // Hardware reset (also wakes the panel from deep sleep) + full register init.
    void init_panel();

    // Full-waveform writes: Display() loads one RAM plane, Base() loads both so
    // that display_partial() has a reference frame. Both blink.
    void display(const uint8_t* fb);
    void display_base(const uint8_t* fb);

    // Partial (no blink) update. Pulses RST itself, which also wakes the panel
    // from deep sleep. Needs a prior display_base().
    void display_partial(const uint8_t* fb);

    // Full white clear (with blink). init_panel() must have been called.
    void clear();

    // Deep sleep: the panel keeps its image with ~zero draw. Any of
    // init_panel()/display_partial() wakes it again.
    void sleep();

    // One refresh cycle with the base/partial policy handled for you: the first
    // call (and the first after clear_screen()) is a full update that
    // establishes the base frame, later ones are partial. Ends with sleep().
    void update(const uint8_t* fb);

    // Wake + full white clear, and forces the next update() to be a full one.
    // Call periodically to avoid ghosting.
    void clear_screen();

private:
    void reset();
    void send_command(uint8_t cmd);
    void send_data(uint8_t data);
    void wait_busy();
    void set_windows(uint16_t x_start, uint16_t y_start, uint16_t x_end, uint16_t y_end);
    void set_cursor(uint16_t x_start, uint16_t y_start);
    void turn_on_display(uint8_t update_control);
    void write_plane(uint8_t ram_command, const uint8_t* fb);

    Epd2in13V4Config m_cfg{};
    bool m_full_refresh_next = true;
};

} // namespace pico_toolset
