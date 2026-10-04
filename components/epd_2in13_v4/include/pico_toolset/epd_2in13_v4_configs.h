#pragma once

#include "pico_toolset/epd_2in13_v4.h"

namespace pico_toolset::configs::epd_2in13_v4 {

// Waveshare Pico-ePaper-2.13 (V4) plugged on the Raspberry Pi Pico / Pico W
// header: the module's fixed wiring (SPI1; CLK GP10, DIN GP11, CS GP9, DC GP8,
// RST GP12, BUSY GP13). Validated by PicoADSB on a Pico W.
inline const Epd2in13V4Config kWavesharePicoEpaper213 = {
    .spi = spi1,
    .pin_sck = 10,
    .pin_mosi = 11,
    .pin_cs = 9,
    .pin_dc = 8,
    .pin_rst = 12,
    .pin_busy = 13,
};

} // namespace pico_toolset::configs::epd_2in13_v4
