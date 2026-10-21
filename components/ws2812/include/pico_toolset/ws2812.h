#pragma once

#include <cstddef>
#include <cstdint>

#include "hardware/pio.h"

namespace pico_toolset {

enum class Ws2812Order : uint8_t { GRB, RGB };

// All fields are board wiring -- no defaults except the signalling rate; see
// ws2812_configs.h for validated presets.
struct Ws2812Config {
    uint8_t     pin;
    uint8_t     count;                 // <= Ws2812::kMaxLeds
    PIO         pio;
    uint8_t     sm;                    // free state machine index in that PIO
    Ws2812Order order;
    // pio_set_gpio_base() value for this PIO (0 or 16), applied before the
    // program loads, or -1 to leave it alone. RP2350B only: pins >= 32 need
    // 16 (see docs/guides/pio-gpio-window.md). Affects every state machine
    // already using this PIO.
    int         gpio_base = -1;
    uint32_t    serial_hz = 800'000;   // WS2812 data rate (SK6812 accept the same)
};

// Addressable-LED chain (WS2812/SK6812) driven by one PIO state machine; the
// colours are buffered, show() clocks them out (blocking until the FIFO has
// accepted them) and the chain latches after >=80 us of idle.
class Ws2812 {
public:
    static constexpr size_t kMaxLeds = 64;

    // False if the program doesn't fit in the PIO or a pin/count is invalid.
    bool init(const Ws2812Config& config);

    void set_rgb(size_t index, uint8_t r, uint8_t g, uint8_t b);
    // h 0-360, s/v 0-1.
    void set_hsv(size_t index, float h, float s, float v);
    void clear();
    void show();

    [[nodiscard]] size_t count() const { return m_cfg.count; }

private:
    Ws2812Config m_cfg{};
    uint32_t m_pixels[kMaxLeds]{};
    bool m_inited = false;
};

} // namespace pico_toolset
