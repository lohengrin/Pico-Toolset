#pragma once

// Known-good PsramConfig presets for specific boards. RP2350-only, like
// psram.h itself.

#include "pico_toolset/psram.h"

#if PICO_RP2350

namespace pico_toolset::configs::psram {

// Waveshare RP2350-PiZero's onboard PSRAM chip. Validated on real hardware
// at 30MHz.
//
// PicoDoom performance work (2026-09): 37.5MHz (clk_sys/4, one divisor step
// down from the original 30MHz/clk_sys-5) measured almost no FPS gain on
// real hardware -- core0-memcpy dropped a little (~18% -> ~15%, roughly
// proportional to the clock bump) but core1-convert (the bigger PSRAM-bound
// cost, reading half its source rows from PSRAM) didn't move, suggesting
// convert's many small/scattered per-pixel reads are QMI
// transaction-overhead-bound rather than bandwidth-bound, unlike memcpy's
// one big sequential burst.
//
// The chip is speced for ~100MHz. PicoDoom.cpp now also overclocks clk_sys
// to 200MHz at boot (before psram_init() runs), so
// psram_configure_params()'s divisor = ceil(clk_sys/max_freq) can hit
// EXACTLY divisor=2 -> 100MHz, right at the chip's spec ceiling with no
// rounding -- requesting 75MHz here (a value computed back when clk_sys was
// still 150MHz) would actually land on divisor=3 -> 66.67MHz today, leaving
// the exact-100MHz option on the table. NOT YET hardware-validated at
// 100MHz specifically (real-hardware testing so far confirmed 37.5MHz and
// 75/66.67MHz-ish territory fine; 100MHz is a further step, right at the
// chip's own rated limit with no margin -- watch closely).
// psram_init()'s self-test (self_test_samples below) catches a clock too
// fast for the chip to respond correctly at init time and halts boot
// (PicoDoom.cpp's fatal("PSRAM detected but self-test failed")), but does
// not rule out intermittent runtime corruption under sustained real
// traffic -- boot-test repeatedly (cold boot + soft reset, not just once)
// and watch for gameplay/WAD-load corruption before trusting this, per this
// project's phased-hardware-rollout practice. Revert to 30'000'000 (and
// drop PicoDoom.cpp's clk_sys overclock) if anything looks wrong. See
// psram_set_clock_hz() (psram.h) for changing this at runtime instead of
// only at psram_init() time.
inline constexpr PsramConfig kWaveshareRp2350PiZero = {
    .cs_pin = 47,
    .max_clock_hz = 100'000'000,
    .run_self_test = true,
    .self_test_samples = 64,
};

} // namespace pico_toolset::configs::psram

#endif // PICO_RP2350
