#pragma once

#include <cstdint>

// Cortex-M33 hard-fault handler that survives a watchdog reset to report
// its diagnostics on the *next* boot, instead of the SDK's default weak
// isr_hardfault (a silent `bkpt #0` -- with no debug probe attached, that
// halts the core with no message at all, indistinguishable from an infinite
// loop). See fault_handler.cpp for why the report can't just be printed
// from fault context (TinyUSB/stdio_usb re-entrancy deadlock) and instead
// goes through the watchdog's scratch registers.
//
// No init call is needed: isr_hardfault/hard_fault_handler_c are compiled-in
// overrides of the SDK's weak defaults purely by being linked in (i.e. by
// linking this library). Only report_pending_hard_fault() needs calling,
// once, from normal boot context.
//
// Scratch-register allocation (watchdog_hw->scratch[0..7], 8 words shared
// across every consumer of this toolset -- see the top-level README for the
// full table): this library uses [2]=magic, [3]=PC, [5]=LR, [6]=CFSR.
// [0]/[1] belong to reset_buttons' watchdog_reboot_with_tag()/
// consume_pending_watchdog_tag(); [4] is reserved by the SDK's own
// watchdog_enable()/watchdog_reboot() bookkeeping; [7] is free.

namespace pico_toolset {

// One fault's diagnostics (ARMv8-M Cortex-M33 SCB fault registers -- see the
// ARMv8-M Architecture Reference Manual).
struct FaultInfo {
    uint32_t pc;   // faulting instruction's address
    uint32_t lr;   // link register at fault entry
    uint32_t cfsr; // Configurable Fault Status Register: what kind of fault
};

// Checks whether a hard fault preceded this boot and, if so, clears the
// marker (so a later plain power-cycle reads back false) and fills `out`.
// Returns false (leaving `out` untouched) if no fault was pending. Safe to
// call from normal boot context only (not fault context) -- see the header
// comment above.
bool consume_pending_hard_fault(FaultInfo& out);

// Convenience wrapper: consumes the pending fault (if any) and printf's it.
// `tag` prefixes the printed lines (e.g. your project's name); pass nullptr
// for a generic "PicoToolset" prefix. Returns whether a fault was pending.
// Call once, from normal boot context (after stdio_init_all() and any
// USB-CDC-attach wait), before other diagnostic output.
bool report_pending_hard_fault(const char* tag = nullptr);

} // namespace pico_toolset
