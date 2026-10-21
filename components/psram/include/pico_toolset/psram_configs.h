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

// Pimoroni Presto's onboard 8 MB APS6404L PSRAM on QMI CS1, CS=GPIO47.
// max_clock_hz is a conservative 50 MHz ceiling (the SDK board header leaves
// sysclk at 150 MHz, giving divider 3): the same chip runs faster on other
// boards, tune upward with the psram_qspi_sweep example. Pin per pico-sdk's
// pimoroni_presto.h (PRESTO_PSRAM_CS_PIN); validated on real hardware by
// examples/presto.
inline constexpr PsramConfig kPimoroniPresto = {
    .cs_pin = 47,
    .max_clock_hz = 50'000'000,
    .run_self_test = true,
    .self_test_samples = 64,
};

} // namespace pico_toolset::configs::psram

#endif // PICO_RP2350
