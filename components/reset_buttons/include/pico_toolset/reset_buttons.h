#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace pico_toolset {

// Debounced, momentary, active-HIGH (pull-down) buttons -- e.g. a small
// bank of model/mode-select buttons on a carrier board. Config-driven: no
// hardcoded pin count or debounce time.
class DebouncedButtons {
public:
    static constexpr int kMaxButtons = 8;

    // Configures each of `pins` as a pulled-down input. Copies `pins` into
    // internal fixed storage (no heap) -- at most kMaxButtons. Returns
    // false (and configures nothing) if pins.size() > kMaxButtons.
    bool init(std::span<const uint8_t> pins, int debounce_frames = 3);

    // Call once per frame/poll cycle. Returns the index (into the `pins`
    // passed to init()) of a button whose debounce threshold was JUST
    // reached this call -- edge-triggered, fires once per press, not once
    // per frame held -- or -1 if none crossed the threshold this call.
    int poll();

    // Instantaneous (non-debounced) read of every configured button as a
    // string of '0'/'1' (pins in configured order, '1' = pressed).
    // Diagnostic only: distinguishes "GPIO never toggles" (wiring) from
    // "toggles but debounce/consumer logic doesn't trigger" (elsewhere).
    [[nodiscard]] std::string raw_state() const;

private:
    uint8_t m_pins[kMaxButtons] = {};
    int     m_press_streak[kMaxButtons] = {};
    int     m_count = 0;
    int     m_debounce_frames = 3;
};

// Reboot into a tagged mode, using the RP2040/RP2350 watchdog's scratch
// registers to carry a caller-defined tag across a watchdog_reboot() --
// they survive that reset but read back 0 on a plain power-on, so a magic
// marker (baked into this pair of functions) distinguishes "rebooted with a
// tag" from "cold power-on" reliably. Reusable for any "boot straight into
// mode X" use case (e.g. a button bank mapping each button to a different
// startup mode without an in-process mode switch) -- does not by itself
// know what a tag value means, only that it survived the reboot.
//
// Never returns (spins waiting for the reboot to take effect, matching
// watchdog_reboot()'s own contract).
[[noreturn]] void watchdog_reboot_with_tag(uint32_t tag);

// Checked once at startup: if a watchdog_reboot_with_tag() reboot is
// pending, returns the tag it was called with via `out_tag` and clears the
// marker (so a later plain power-cycle reads back false again). Returns
// false on a normal power-on reset.
bool consume_pending_watchdog_tag(uint32_t& out_tag);

} // namespace pico_toolset
