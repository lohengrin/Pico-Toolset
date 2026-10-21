#pragma once

// Known-good PiezoConfig presets for specific boards.

#include "pico_toolset/piezo.h"

namespace pico_toolset::configs::piezo {

// Pimoroni Presto's on-board piezo speaker, driven through a transistor from
// GPIO43 (PRESTO_BUZZER_PIN). Validated on real hardware by examples/presto
// (wiring per Pimoroni's examples/buzzer.py).
inline constexpr PiezoConfig kPimoroniPresto = {
    .pin = 43,
};

} // namespace pico_toolset::configs::piezo
