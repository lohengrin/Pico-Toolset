#pragma once

// Known-good Ili9486Config presets for specific board+panel combinations.
// Each preset here has been validated on real hardware by a consumer of
// this toolset -- see the comment on each one for the board/wiring.
//
// Usage: pico_toolset::Ili9486 lcd; lcd.init(pico_toolset::configs::ili9486::kWaveshareRp2350PiZero);
// To adapt a preset for a similar-but-not-identical board, copy it and
// change only the fields that differ -- don't mutate these constants.

#include "pico_toolset/ili9486.h"

namespace pico_toolset::configs::ili9486 {

// Waveshare RP2350-PiZero + an external 3.5" ILI9486 SPI LCD (the 16-bit
// shift-register variant, not the 8080-parallel one) wired over the board's
// GPIO/SPI header -- the board itself has no built-in screen (its own
// display path is the PIO-driven HDMI/DVI output, see pico_toolset_dvi_hdmi).
// Validated on real hardware.
inline const Ili9486Config kWaveshareRp2350PiZero = {
    .spi_instance = spi1,
    .pin_sck = 10,
    .pin_mosi = 11,
    .pin_miso = 12, // shared with the board's XPT2046 touch controller
    .pin_cs = 8,
    .pin_dc = 24,
    .pin_rst = 25,
    .pin_backlight = 255, // no PWM backlight pin wired on this board
    .spi_freq_hz = 8'000'000,
    .pixel_freq_hz = 25'000'000,
    .use_dma = true,
    .dma_channel = -1, // auto-claim
};

} // namespace pico_toolset::configs::ili9486
