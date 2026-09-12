#pragma once

// Known-good SdCardConfig presets for specific boards. Both presets here
// use PIO-bit-banged SPI (spi_instance = nullptr) since neither board wires
// its uSD socket's native 4-bit SDIO pins to a hardware-SPI-capable pin set.

#include "pico_toolset/sdcard.h"

namespace pico_toolset::configs::sdcard {

// Waveshare RP2350-PiZero's uSD socket (SD-in-SPI-mode over its native SDIO
// wiring: CS=D3/GPIO43, MOSI=CMD/GPIO31, MISO=D0/GPIO40, SCK=CLK/GPIO30).
// gpio_base=16 is required, not optional -- MISO (40) is outside PIO's
// default 0-31 addressing window. PIO1/SM0: PIO0 is reserved for this
// board's Pico-PIO-USB port. Validated on real hardware by TOM6809
// (github.com/lohengrin/TOM6809).
inline const SdCardConfig kWaveshareRp2350PiZero = {
    .spi_instance = nullptr,
    .pin_miso = 40,
    .pin_cs = 43,
    .pin_sck = 30,
    .pin_mosi = 31,
    .pullup = true,
    .clk_slow_hz = 100'000,
    .clk_fast_hz = 10'000'000,
    .pio = pio1,
    .sm = 0,
    .gpio_base = 16,
};

// TOM6809's custom "Pico DV" carrier board's uSD socket (SD-in-SPI-mode
// over its native SDIO wiring: CS=DAT3/GPIO22, MOSI=CMD/GPIO18,
// MISO=DAT0/GPIO19, SCK=CLK/GPIO5). PIO1/SM0, to avoid colliding with this
// board's I2S audio, which defaults to PIO0/SM0. Validated on real hardware
// by TOM6809.
inline const SdCardConfig kPicoDvCarrier = {
    .spi_instance = nullptr,
    .pin_miso = 19,
    .pin_cs = 22,
    .pin_sck = 5,
    .pin_mosi = 18,
    .pullup = true,
    .clk_slow_hz = 100'000,
    .clk_fast_hz = 10'000'000,
    .pio = pio1,
    .sm = 0,
    .gpio_base = -1, // every pin here is within the default 0-31 window
};

} // namespace pico_toolset::configs::sdcard
