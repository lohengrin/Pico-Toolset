#pragma once

#include <cstdint>

namespace pico_toolset {

// Maps Boot-Protocol keyboard key usages to printable ASCII (US layout).
// Returns 0 for non-printable keys (modifiers, arrows, F-keys, ...).
// `modifier_mask` is the Boot-Protocol ModifierFlags byte (bit0 LEFT_CTRL
// ... bit6 RIGHT_ALT); shift flips letters to upper case and picks the
// shifted symbol on each non-letter key.
uint8_t hid_usage_to_ascii(uint8_t usage_id, uint8_t modifier_mask);

} // namespace pico_toolset