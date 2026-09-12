#pragma once

// Known-good Xpt2046Config presets for specific board+panel combinations.
// spi_instance is set here to match the co-located display's default SPI
// bus, but when initializing alongside a real display driver instance,
// prefer overriding it from that driver's own spi() accessor (e.g.
// `auto cfg = configs::xpt2046::kWaveshareRp2350PiZero; cfg.spi_instance = lcd.spi();`)
// so the two never disagree about which bus they share.

#include "pico_toolset/xpt2046.h"

namespace pico_toolset::configs::xpt2046 {

// Waveshare RP2350-PiZero + the resistive touch panel of an external 3.5"
// LCD wired over the board's GPIO/SPI header (the board has no built-in
// screen), sharing the SPI1 bus with
// pico_toolset::configs::ili9486::kWaveshareRp2350PiZero (ili9486_configs.h).
// Validated on real hardware.
inline const Xpt2046Config kWaveshareRp2350PiZero = {
    .spi_instance = spi1,
    .pin_cs = 7,
    .pin_irq = 17,
    .touch_freq_hz = 2'000'000,
};

// Elecrow CrowPanel PICO HMI 2.8" built-in resistive touch, sharing SPI1
// with the panel's ST7789 display (configs::st7789::kElecrowCrowPanelPicoHmi28)
// and its uSD card (configs::sdcard::kElecrowCrowPanelPicoHmi28) -- each on
// its own CS line (touch CS=GP16, PENIRQ=GP17). Pins from the board's
// schematic; bench-confirm on first flash (this board's touch path is new,
// unlike its already-flying display).
inline const Xpt2046Config kElecrowCrowPanelPicoHmi28 = {
    .spi_instance = spi1,
    .pin_cs = 16,
    .pin_irq = 17,
    .touch_freq_hz = 2'000'000,
};

} // namespace pico_toolset::configs::xpt2046
