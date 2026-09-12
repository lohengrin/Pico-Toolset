#pragma once

// Known-good St7789Config presets for specific board+panel combinations.

#include "pico_toolset/st7789.h"

namespace pico_toolset::configs::st7789 {

// Elecrow CrowPanel PICO HMI 2.8" (RP2040, 320x240 SPI TFT, ST7789-family
// controller). SPI1 shared with the panel's XPT2046 touch controller
// (configs::xpt2046::kElecrowCrowPanelPicoHmi28) and uSD card
// (configs::sdcard::kElecrowCrowPanelPicoHmi28) -- each device has its own
// CS line on the same bus. Pins from the board's schematic; the display
// path itself is validated on real hardware (ported from a consumer's
// working driver). madctl=0x70 (COL_ORDER | SWAP_XY | SCAN_ORDER) + no
// color inversion + tearing-effect off matches that consumer's validated
// non-rotated orientation for this exact panel.
inline const St7789Config kElecrowCrowPanelPicoHmi28 = {
    .spi_instance = spi1,
    .pin_sck = 10,
    .pin_mosi = 11,
    .pin_cs = 9,
    .pin_dc = 8,
    .pin_reset = 15,
    .pin_backlight = 18,
    .width = 320,
    .height = 240,
    .col_offset = 0,
    .row_offset = 0,
    .madctl = 0x70,
    .tearing_effect_on = false,
    .inversion_on = false,
    .spi_freq_hz = 62'500'000,
};

} // namespace pico_toolset::configs::st7789
