#pragma once

// Known-good UsbHidConfig presets for specific board+use-case combinations.
// Two presets exist for the same board here because pio_num/run_on_core1
// depend on what ELSE is active on that board, not just the board itself --
// pick the one matching your build, or copy and adjust.

#include "pico_toolset/usb_hid_host.h"

namespace pico_toolset::configs::usb_hid {

// Waveshare RP2350-PiZero's PIO-USB port (GPIO28=D+/GPIO29=D-), for a build
// where core1 is free to dedicate to the USB host stack and PIO0 is free
// (i.e. no DVI/HDMI output active). Validated on real hardware by TOM6809
// (github.com/lohengrin/TOM6809)'s LCD profile.
inline constexpr UsbHidConfig kWaveshareRp2350PiZeroLcd = {
    .pin_dp = 28,
    .pio_num = 0,
    .run_on_core1 = true,
    .core1_stack_size = 4096,
};

// Same board and pins, for a build where core1 is already dedicated to a
// DVI/HDMI video driver (so the host stack must run on core0 instead, polled
// from the app's own loop via UsbHidHost::task()) and PIO0 is taken by that
// same DVI driver (so USB moves to PIO2 -- PIO1 is typically the SD card's).
// Validated on real hardware by TOM6809's HDMI profile.
inline constexpr UsbHidConfig kWaveshareRp2350PiZeroHdmi = {
    .pin_dp = 28,
    .pio_num = 2,
    .run_on_core1 = false,
};

} // namespace pico_toolset::configs::usb_hid
