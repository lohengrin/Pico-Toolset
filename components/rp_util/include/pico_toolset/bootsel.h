#pragma once

namespace pico_toolset {

// True while the BOOTSEL button is held. The button shares the flash chip-select line, so a
// read floats that line for ~20 us with interrupts off: the function runs from RAM, and
// nothing may execute from flash or XIP-mapped PSRAM meanwhile -- that includes the other core
// (a core that only runs RAM code, like a scanout/IRQ core, is fine). Poll it at a modest
// rate (tens of ms) from the main loop; there is no debounce.
bool bootsel_pressed();

} // namespace pico_toolset
