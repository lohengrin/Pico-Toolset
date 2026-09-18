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

// Waveshare RP2350-PiZero + a SunFounder 3.5" 480x320 IPS SPI LCD
// (ST7796U + XPT2046 touch) wired over the board's GPIO/SPI header,
// replacing an ILI9486 panel on the same wiring (the board itself has no
// built-in screen -- its own display path is the PIO-driven HDMI/DVI
// output, see pico_toolset_dvi_hdmi). Pins carried over unchanged from the
// validated configs::ili9486::kWaveshareRp2350PiZero preset (same board,
// same GPIO/SPI header, only the panel's controller chip changed) --
// hardware-validated 2026-09. `madctl` (MV|BGR, via St7796Orientation) gives
// correct landscape orientation with no mirroring; `invert_colors=true` is
// this specific panel's own requirement (not every ST7796U panel needs it --
// see St7796Config::invert_colors). `spi_freq_hz` is kept at the ILI9486
// preset's already-validated 33.33MHz rather than this panel's much higher
// 125MHz spec ceiling -- raise it once bench-tested, see
// St7796::set_pixel_clock_hz().
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
    .madctl = St7796Orientation{.swap_row_column = true,
                                 .color_order = St7796ColorOrder::Bgr}.to_madctl_byte(),
    .invert_colors = true, // this panel batch needs it -- see St7796Config's doc comment
    .spi_freq_hz = 33'333'333, // matches the validated ILI9486 preset's achieved rate; raise once bench-tested
    .use_dma = true,
    .dma_channel = -1, // auto-claim
};

} // namespace pico_toolset::configs::st7796
