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

// Same board and pins as kWaveshareRp2350PiZeroLcd (PIO0 free), but for a
// consumer that launches and owns core1 itself instead of letting init()
// spawn it -- e.g. to interleave UsbHidHost::task() with its own per-frame
// work (a chunked LCD blit state machine) in one core1 loop, the way a
// display driver's DMA-fed pixel push can't be split across a core boundary
// mid-frame. With run_on_core1=false, init() only calls tuh_init() (on
// whichever core calls it -- must be the SAME core that then calls task(),
// since Pico-PIO-USB's SOF-timer IRQ handler binds to the core that
// registered it): call init() from inside your own core1 entry function,
// then loop task() + your other work from there, mirroring the pattern
// TOM6809/kWaveshareRp2350PiZeroHdmi already uses on core0. Validated on
// real hardware by PicoDoom (github.com/lohengrin/PicoDoom), whose core1
// loop also drives its ILI9486 driver's chunked blit.
inline constexpr UsbHidConfig kWaveshareRp2350PiZeroLcdManualCore1 = {
    .pin_dp = 28,
    .pio_num = 0,
    .run_on_core1 = false,
};

} // namespace pico_toolset::configs::usb_hid
