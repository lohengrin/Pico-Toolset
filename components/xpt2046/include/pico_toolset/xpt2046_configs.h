#pragma once

// Known-good Xpt2046Config presets for specific board+panel combinations.
// spi_instance is set here to match the co-located display's default SPI
// bus, but when initializing alongside a real display driver instance,
// prefer overriding it from that driver's own spi() accessor (e.g.
// `auto cfg = configs::xpt2046::kWaveshareRp2350PiZero; cfg.spi_instance = lcd.spi();`)
// so the two never disagree about which bus they share.

#include "pico_toolset/xpt2046.h"

namespace pico_toolset::configs::xpt2046 {

// Waveshare RP2350-PiZero + its bundled 3.5" LCD's resistive touch panel,
// sharing the SPI1 bus with pico_toolset::configs::ili9486::kWaveshareRp2350PiZero
// (ili9486_configs.h). Validated on real hardware.
inline const Xpt2046Config kWaveshareRp2350PiZero = {
    .spi_instance = spi1,
    .pin_cs = 7,
    .pin_irq = 17,
    .touch_freq_hz = 2'000'000,
};

} // namespace pico_toolset::configs::xpt2046
