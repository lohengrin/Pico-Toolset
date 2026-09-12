#pragma once

#include <cstddef>
#include <cstdint>

#include <memory_resource>

// PSRAM support targets the RP2350 only (its QSPI QMI CS1 interface).
// On RP2040 the whole library is a no-op: psram_init() returns a status with
// present=false.
#if PICO_RP2350
#include "hardware/psram.h"

namespace pico_toolset {

// Configuration for PSRAM bring-up. cs_pin has no working default -- it's
// board wiring (which QSPI CS pin the PSRAM chip is on), not driver
// behavior -- see psram_configs.h for known-good per-board presets (e.g.
// configs::psram::kWaveshareRp2350PiZero), or set it yourself for a board without
// one yet.
struct PsramConfig {
    uint8_t  cs_pin;                    // QSPI CS pin for the PSRAM chip -- must be set
    uint32_t max_clock_hz = 30'000'000; // PSRAM clock (0 = SDK default)
    bool     run_self_test = true;     // Run read/write pattern test
    size_t   self_test_samples = 64;   // Number of test samples
};

// Diagnostics for one psram_init() call.
struct PsramStatus {
    bool present = false;  // chip detected (responded to the QSPI ID read)
    bool test_ok = false;  // detected AND the read/write self-test passed
    size_t size_bytes = 0; // 0 if not present
    size_t self_test_samples = 0; // number of samples used at test time

    // Diagnostics for the first failing sample, valid only when present and
    // !test_ok.
    size_t fail_offset = 0;
    uint8_t fail_expected = 0;
    uint8_t fail_actual = 0;
    // Which write at that offset failed: false = the address-derived pattern,
    // true = its complement.
    bool fail_on_second_pattern = false;
    uint32_t clk_sys_hz_at_test = 0;
};

// Detects the PSRAM chip on `config.cs_pin`, configures the QMI CS1 memory
// map, and runs the sampled read/write self-test. Call exactly once, before
// any psram_malloc().
//
// Internally protected against hardware_psram's documented unsafety
// (psram_detect_cs_and_size()/psram_reinitialize() briefly make flash
// unreadable via XIP, so an interrupt handler firing -- or the other core
// executing from flash concurrently -- during that window hangs or faults):
// uses flash_safe_execute() when the other core is lockout-ready (has
// called flash_safe_execute_core_init()/multicore_lockout_victim_init() --
// e.g. automatically, if it's running pico_toolset_usb_hid's host stack,
// see that component's host_stack_setup()), otherwise falls back to a plain
// interrupt-disable, which is correct as long as the other core hasn't
// started running anything yet. If you call this after launching a core1
// workload that is NOT pico_toolset_usb_hid, make sure that core has called
// flash_safe_execute_core_init() itself first, or this falls back to the
// interrupt-only protection and the other-core race is still possible.
PsramStatus psram_init(const PsramConfig& config);

// Status from the last psram_init() (all-false default if it hasn't run).
const PsramStatus& psram_status();

// First-fit free-list allocator over the mapped region. Returns nullptr if
// PSRAM isn't present/healthy, on OOM, or for size 0. Blocks are 8-byte
// aligned.
extern "C" void* psram_malloc(size_t size);
extern "C" void psram_free(void* ptr);

// Bytes currently handed out and not yet freed. 0 if PSRAM isn't healthy.
size_t psram_used_bytes();

// std::pmr::memory_resource wrapping psram_malloc()/psram_free() -- lets
// std::pmr::vector and friends allocate out of PSRAM. do_allocate() ignores
// alignment beyond the fixed 8-byte block alignment.
class PsramResource : public std::pmr::memory_resource {
protected:
    void* do_allocate(size_t bytes, size_t /*alignment*/) override {
        void* p = psram_malloc(bytes);
        if (!p) {
            // PSRAM exhausted: the standard says to throw bad_alloc, but the
            // toolchain builds with -fno-exceptions (pico-sdk default), so
            // trap instead of returning a bogus pointer.
            __asm__ volatile("bkpt #0");
            __builtin_unreachable();
        }
        return p;
    }
    void do_deallocate(void* p, size_t /*bytes*/, size_t /*alignment*/) override { psram_free(p); }
    [[nodiscard]] bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override {
        return this == &other;
    }
};

} // namespace pico_toolset

#endif // PICO_RP2350