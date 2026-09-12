#pragma once

// Known-good PsramConfig presets for specific boards. RP2350-only, like
// psram.h itself.

#include "pico_toolset/psram.h"

#if PICO_RP2350

namespace pico_toolset::configs::psram {

// Waveshare RP2350-PiZero's onboard PSRAM chip. Validated on real hardware.
inline constexpr PsramConfig kWaveshareRp2350PiZero = {
    .cs_pin = 47,
    .max_clock_hz = 30'000'000,
    .run_self_test = true,
    .self_test_samples = 64,
};

} // namespace pico_toolset::configs::psram

#endif // PICO_RP2350
