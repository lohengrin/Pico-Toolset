#pragma once

// Known-good PsramConfig presets for specific boards. RP2350-only, like
// psram.h itself.

#include "pico_toolset/psram.h"

#if PICO_RP2350

namespace pico_toolset::configs::psram {

// Waveshare RP2350-PiZero's onboard PSRAM chip. Validated on real hardware.
//
// max_clock_hz targets a QMI CS1 divisor of 2 (clk_sys/2, the fastest this
// component's rxdelay-limited divisor scheme can reach short of divisor=1,
// which the RXDELAY clamp never allows -- see psram.cpp's clamp_max_freq_hz())
// on this consumer's two known clk_sys values, 264MHz (LCD profile) and
// 252MHz (HDMI profile): divisor = ceil(clk_sys/max_clock_hz), so
// 132'000'000 (264MHz/2) gives divisor 2 on both -- ceil(264/132)=2 exactly,
// ceil(252/132)=2 (252/132=1.9...). Real-hardware-confirmed at divisor 2 on
// this board.
inline constexpr PsramConfig kWaveshareRp2350PiZero = {
    .cs_pin = 47,
    .max_clock_hz = 132'000'000,
    .run_self_test = true,
    .self_test_samples = 64,
};

} // namespace pico_toolset::configs::psram

#endif // PICO_RP2350
