#pragma once

// Known-good St7796Config presets for specific board+panel combinations.
// Each preset here has been validated on real hardware by a consumer of
// this toolset, unless its own comment says otherwise -- see that comment
// for details.
//
// Usage: pico_toolset::St7796 lcd; lcd.init(pico_toolset::configs::st7796::kWaveshareRp2350PiZero);
// To adapt a preset for a similar-but-not-identical board, copy it and
// change only the fields that differ -- don't mutate these constants.

#include "pico_toolset/st7796.h"

namespace pico_toolset::configs::st7796 {

// Waveshare RP2350-PiZero + an external 3.5" ST7796U SPI LCD wired over the
// board's GPIO/SPI header, replacing an ILI9486 panel on the same wiring
// (the board itself has no built-in screen -- its own display path is the
// PIO-driven HDMI/DVI output, see pico_toolset_dvi_hdmi). Pins are carried
// over unchanged from the validated configs::ili9486::kWaveshareRp2350PiZero
// preset (same board, same GPIO/SPI header, only the panel's controller
// chip changed) -- NOT yet independently hardware-validated for the ST7796U
// panel itself: bench-confirm on first flash, in particular `madctl` (the
// value below, 0x28 = MV|BGR, mirrors ILI9486's landscape orientation on
// this board bit-for-bit, since both controllers share the same MADCTL bit
// assignments -- but confirm the image isn't mirrored/rotated before
// trusting it) and `spi_freq_hz` (kept at the ILI9486 preset's
// already-validated 33.33MHz rather than this panel's much higher 125MHz
// spec ceiling -- raise it once bench-tested, see St7796::set_pixel_clock_hz()).
inline const St7796Config kWaveshareRp2350PiZero = {
    .spi_instance = spi1,
    .pin_sck = 10,
    .pin_mosi = 11,
    .pin_miso = 12, // shared with the board's XPT2046 touch controller
    .pin_cs = 8,
    .pin_dc = 24,
    .pin_rst = 25,
    .pin_backlight = 255, // no PWM backlight pin wired on this board
    .width = 480,
    .height = 320,
    .col_offset = 0,
    .row_offset = 0,
    .madctl = 0x28, // MV|BGR -- landscape, matching the ILI9486 preset's orientation; bench-confirm
    .spi_freq_hz = 33'333'333, // matches the validated ILI9486 preset's achieved rate; raise once bench-tested
    .use_dma = true,
    .dma_channel = -1, // auto-claim
};

} // namespace pico_toolset::configs::st7796
