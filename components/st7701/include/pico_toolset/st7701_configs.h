#pragma once

// Known-good St7701Config presets. RP2350-only, like st7701.h itself.

#include "pico_toolset/st7701.h"

#if PICO_RP2350

namespace pico_toolset::configs::st7701 {

// Pimoroni Presto's built-in 480x480 panel (TL040WVS03CT15-H1263A), at the
// half-resolution (240x240, scanout doubles every pixel/line) default.
// Pins from Pimoroni's Presto firmware and pico-sdk's pimoroni_presto.h
// (PRESTO_LCD_*): RGB565 data on GPIO1-16, HSYNC 19, VSYNC 20, DE 21,
// DOTCLK 22, init SPI1 CLK26/DAT27/CS28, backlight PWM 45. PIO1 matches the
// firmware (WS2812 lives on PIO0). Validated on real hardware by examples/presto. `framebuffer` must be set by the caller
// (240*240 uint16_t in SRAM).
inline const St7701Config kPimoroniPrestoHalfRes = {
    .width = 240,
    .height = 240,
    .rotate_180 = false,
    .pin_d0 = 1,
    .pin_hsync = 19,
    .pin_vsync = 20,
    .pin_de = 21,
    .pin_dot_clk = 22,
    .spi = spi1,
    .pin_spi_cs = 28,
    .pin_spi_clk = 26,
    .pin_spi_dat = 27,
    .pin_backlight = 45,
    .pio = pio1,
    .framebuffer = nullptr,
};

// Same panel at native 480x480 (a 450 KB SRAM framebuffer; PSRAM cannot be
// scanned out from, so the app's own data goes there instead).
inline const St7701Config kPimoroniPrestoFullRes = {
    .width = 480,
    .height = 480,
    .rotate_180 = false,
    .pin_d0 = 1,
    .pin_hsync = 19,
    .pin_vsync = 20,
    .pin_de = 21,
    .pin_dot_clk = 22,
    .spi = spi1,
    .pin_spi_cs = 28,
    .pin_spi_clk = 26,
    .pin_spi_dat = 27,
    .pin_backlight = 45,
    .pio = pio1,
    .framebuffer = nullptr,
};

} // namespace pico_toolset::configs::st7701

#endif // PICO_RP2350
