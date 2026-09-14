// PSRAM QSPI bus-speed example (RP2350 only): finds the highest reliable
// PSRAM (M1) serial clock the QSPI controller can sustain by sweeping the
// M1 clock (from 50MHz in ~10MHz hops) -- not the CPU clock. clk_sys stays
// capped at ~300MHz: the thing under test is the PSRAM bus, not the RP2350
// itself. M1 runs at clk_sys/divisor, and the SDK refuses divisor 1 above
// 100MHz clk_sys, so each step re-tunes the driver's psram_set_clock_hz()
// to pin a specific divisor and then verifies both chips sharing the QMI:
// the flash M0 window (this board runs it at clk_sys/2, its boot-time
// divider) and the PSRAM M1 window. Reports the highest fully-passing M1
// clock and a safe value with margin, applies it, and idles there.
//
// Why the whole binary must run from SRAM (pico_set_binary_type copy_to_ram
// in CMakeLists): flash and PSRAM share the RP2350 QMI/QSPI controller, and
// past some clk_sys the bus stops returning usable data on one or both
// chips. If instructions were being fetched from flash, that first bad fetch
// would crash the CPU before the failing step could be identified. copy_to_ram
// keeps every instruction fetch (including TinyUSB's stdio) in SRAM and treats
// the chips as pure data devices, so a failing step is detected and rolled
// back instead of hung on.
//
// The XIP cache matters as much as the clock: on RP2350 it is not read-only
// -- it buffers pending PSRAM writes and can serve stale flash lines. An
// integrity pass that ignores it measures the cache, not the bus. Every pass
// here is therefore wrapped in xip_cache_clean_all() (commit pending writes
// to PSRAM over the wire) followed by xip_cache_invalidate_all() (force the
// read-backs off the wire).

#include "pico_toolset/psram.h"
#include "pico_toolset/psram_configs.h"

#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/structs/qmi.h"
#include "hardware/vreg.h"
#include "hardware/xip_cache.h"
#include "pico/stdlib.h"

#include <cstdint>
#include <cstdio>

namespace {

// Mirror of the pico_toolset_psram driver's fixed origin (XIP_BASE + this
// board's 16MB flash size); the extent comes from PsramStatus::size_bytes.
constexpr uintptr_t kPsramBase = 0x11000000;

// The flash image itself is never written while running, so a checksum over
// a fixed slice is a stable reference for the M0 window.
constexpr uintptr_t kFlashImageBase = 0x10000000;

constexpr size_t kMaxSweepClocks = 128;

// Harness parameters. cs_pin is board wiring and has no default, exactly
// like the driver's own config structs -- take it from a
// configs::psram::<board> preset, don't invent one.
struct SweepConfig {
    uint8_t cs_pin;

    uint32_t sys_clock_min_hz = 150'000'000;       // sweep floor (snapped up to an attainable PLL value)
    uint32_t sys_clock_max_hz = 300'000'000;       // sweep ceiling raised to 300MHz so M1 = clk_sys/2 reaches
                                                   // 150MHz -- the CPU clock still is not under test, but at
                                                   // divisor 2 (the SDK's floor above 100MHz clk_sys) 150MHz
                                                   // QSPI needs 300MHz clk_sys behind it
    uint32_t clk_sys_step_hz = 10'000'000;         // decimate the PLL grid; finer = denser M1 coverage
    uint8_t min_m1_divisor = 2;                    // SDK forces divisor >= 2 above 100MHz clk_sys, so
                                                   // divisor 1 (M1 == clk_sys) is unreachable here
    uint8_t max_m1_divisor = 7;                    // QMI_M1_TIMING_CLKDIV is a 3-bit field
    uint32_t m1_min_hz = 50'000'000;               // lowest M1 (QSPI/PSRAM) frequency tested
    uint32_t m1_step_hz = 10'000'000;              // ~this spacing between tested M1 frequencies
    uint32_t margin_hz = 10'000'000;               // subtracted from the max stable M1 clock for the safe setting
    size_t flash_check_bytes = 64 * 1024;          // slice of the flash image verified per step
    bool raise_core_voltage = true;                // a bit of headroom never hurts at 300MHz
};

// Every frequency the sys PLL can reach between min and max, ascending, via
// the same search pico-sdk's check_sys_clock_khz() uses -- every entry is a
// frequency set_sys_clock_khz() is guaranteed to accept. Arbitrary MHz steps
// are NOT: 280MHz, for example, has no PLL solution. The raw grid is dense
// (sub-MHz spacings near the sweep floor); step_hz decimates it to coarse
// hops -- keep the first attainable entry at or above a running target, then
// jump the target forward by step_hz -- so each step moves the clock
// meaningfully instead of spending a PLL+QMI re-tune on a small increment.
size_t enumerate_sys_clocks(uint32_t min_hz, uint32_t max_hz, uint32_t step_hz, uint32_t* out, size_t cap) {
    const uint32_t reference_hz = XOSC_HZ / PLL_SYS_REFDIV;
    size_t n = 0;
    for (uint32_t fbdiv = 320; fbdiv >= 16; --fbdiv) {
        const uint32_t vco_hz = fbdiv * reference_hz;
        if (vco_hz < PICO_PLL_VCO_MIN_FREQ_HZ || vco_hz > PICO_PLL_VCO_MAX_FREQ_HZ)
            continue;
        for (uint32_t postdiv1 = 7; postdiv1 >= 1; --postdiv1) {
            for (uint32_t postdiv2 = postdiv1; postdiv2 >= 1; --postdiv2) {
                const uint32_t out_hz = vco_hz / (postdiv1 * postdiv2);
                if (vco_hz % (postdiv1 * postdiv2) != 0 || out_hz < min_hz || out_hz > max_hz)
                    continue;
                bool dup = false;
                for (size_t i = 0; i < n; ++i)
                    if (out[i] == out_hz) { dup = true; break; }
                if (!dup && n < cap)
                    out[n++] = out_hz;
            }
        }
    }
    for (size_t i = 1; i < n; ++i) { // insertion sort; n is small
        const uint32_t v = out[i];
        size_t j = i;
        while (j > 0 && out[j - 1] > v) { out[j] = out[j - 1]; --j; }
        out[j] = v;
    }
    size_t m = 0;
    for (size_t i = 0; i < n; ++i) {
        if (m != 0 && out[i] < out[m - 1] + step_hz)
            continue;
        out[m++] = out[i];
    }
    return m;
}

// One sweep step: a clk_sys value and the M1 divisor pinned at it. The M1
// clock the QSPI bus actually runs at is clk_sys/divisor.
struct SweepEntry {
    uint32_t sys_clk_hz;
    uint8_t divisor;
    uint32_t m1_clk_hz;
};

// Cross-product of decimated clk_sys values and M1 divisors {max_div..min_div}
// covering M1 from min_m1_hz upward, decimated to ~m1_step_hz hops, sorted by
// M1 so the sweep climbs the quantity actually under test -- the QSPI/PSRAM
// frequency -- rather than the CPU clock. M1 = clk_sys/divisor; the last
// step is always kept so the fastest attainable bus speed gets measured too.
// Requesting psram_set_clock_hz(clk_sys/divisor) makes the SDK pick exactly
// that divisor (ceil(clk_sys / (clk_sys/divisor)) == divisor).
size_t build_sweep(const uint32_t* clks, size_t nclk, uint8_t min_div, uint8_t max_div,
                   uint32_t min_m1_hz, uint32_t m1_step_hz,
                   SweepEntry* out, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; i < nclk; ++i)
        for (uint8_t div = max_div; div >= min_div; --div)
            if (clks[i] / div >= min_m1_hz && n < cap)
                out[n++] = {clks[i], div, clks[i] / div};
    for (size_t i = 1; i < n; ++i) { // insertion sort by M1; ties keep the cheaper clk_sys
        const SweepEntry v = out[i];
        size_t j = i;
        while (j > 0 &&
               (out[j - 1].m1_clk_hz > v.m1_clk_hz ||
                (out[j - 1].m1_clk_hz == v.m1_clk_hz && out[j - 1].sys_clk_hz > v.sys_clk_hz))) {
            out[j] = out[j - 1];
            --j;
        }
        out[j] = v;
    }
    size_t m = 0;
    for (size_t i = 0; i < n; ++i) {
        if (m != 0 && out[i].m1_clk_hz < out[m - 1].m1_clk_hz + m1_step_hz)
            continue;
        out[m++] = out[i];
    }
    if (n > 0 && (m == 0 || out[m - 1].m1_clk_hz != out[n - 1].m1_clk_hz))
        out[m++] = out[n - 1]; // the tabled last step -- the fastest attainable M1
    return m;
}

// xorshift32 PRNG for the pattern passes.
uint32_t prng_next(uint32_t* state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return *state = x;
}

// The live M1 clock as configured in the hardware -- ground truth for what
// the PSRAM serial interface actually runs at. (psram_status().clock_hz can
// be optimistic at divisor 1: the SDK's own internal clamp bumps it to 2
// without the toolset knowing, so read the register instead.)
uint32_t applied_m1_clock_hz() {
    const uint32_t divisor = (qmi_hw->m[1].timing & QMI_M1_TIMING_CLKDIV_BITS) >> QMI_M1_TIMING_CLKDIV_LSB;
    return clock_get_hz(clk_sys) / divisor;
}

uint32_t flash_checksum(size_t bytes) {
    // Stale flash lines cached during earlier steps (or the copy_to_ram
    // image load) would answer this read from the cache and falsely pass the
    // M0 window -- force a bus fetch.
    xip_cache_invalidate_all();
    volatile const uint32_t* p = reinterpret_cast<volatile const uint32_t*>(kFlashImageBase);
    uint32_t sum = 0;
    for (size_t i = 0; i < bytes / sizeof(uint32_t); ++i)
        sum += p[i];
    return sum;
}

struct PsramFail {
    bool failed = false;
    size_t offset = 0;
    uint32_t expected = 0;
    uint32_t actual = 0;
};

// Write a pattern into every PSRAM word, flush the pending writes out over
// the bus, then read it all back with the cache forced off so both halves
// really exercise the QSPI interface (see the file-top comment).
bool psram_verify(volatile uint32_t* base, size_t words, PsramFail& fail) {
    // Pass 1: full-region PRBS32.
    uint32_t seed = 0x12345678;
    for (size_t i = 0; i < words; ++i)
        base[i] = prng_next(&seed);
    xip_cache_clean_all();
    xip_cache_invalidate_all();
    seed = 0x12345678;
    for (size_t i = 0; i < words; ++i) {
        const uint32_t expected = prng_next(&seed);
        if (base[i] != expected) {
            fail = {true, i, expected, base[i]};
            return false;
        }
    }

    // Pass 2: alternating 0x55555555 / 0xAAAAAAAA.
    for (size_t i = 0; i < words; ++i)
        base[i] = (i & 1u) ? 0xAAAAAAAA : 0x55555555;
    xip_cache_clean_all();
    xip_cache_invalidate_all();
    for (size_t i = 0; i < words; ++i) {
        const uint32_t expected = (i & 1u) ? 0xAAAAAAAA : 0x55555555;
        if (base[i] != expected) {
            fail = {true, i, expected, base[i]};
            return false;
        }
    }

    // Pass 3: walking-1s (single bit ramps across the whole region).
    for (size_t i = 0; i < words; ++i)
        base[i] = 1u << (i & 31u);
    xip_cache_clean_all();
    xip_cache_invalidate_all();
    for (size_t i = 0; i < words; ++i) {
        const uint32_t expected = 1u << (i & 31u);
        if (base[i] != expected) {
            fail = {true, i, expected, base[i]};
            return false;
        }
    }

    return true;
}

void print_mhz(uint32_t hz) {
    printf("%u.%03u MHz", hz / 1000000, (hz / 1000) % 1000);
}

} // namespace

int main() {
    stdio_init_all();
    sleep_ms(2000);

    using pico_toolset::psram_init;
    using pico_toolset::psram_set_clock_hz;
    using pico_toolset::psram_status;

    SweepConfig cfg;
    // Board wiring from the psram component's validated preset; the sweep
    // re-tunes the M1 clock itself, so the preset's 30MHz boot max is fine.
    cfg.cs_pin = pico_toolset::configs::psram::kWaveshareRp2350PiZero.cs_pin;

    if (cfg.raise_core_voltage) {
        vreg_set_voltage(VREG_VOLTAGE_1_25); // headroom for the 300MHz cap of the sweep
        sleep_ms(10);
    }

    const uint32_t original_clk = clock_get_hz(clk_sys);
    const auto st = psram_init(pico_toolset::configs::psram::kWaveshareRp2350PiZero);
    if (!st.present || !st.test_ok) {
        printf("PSRAM not usable: present=%d size=%zu test_ok=%d\n", st.present, st.size_bytes, st.test_ok);
        for (;;) tight_loop_contents();
    }

    const uint32_t flash_reference = flash_checksum(cfg.flash_check_bytes);
    printf("boot clk_sys %u MHz, PSRAM %zu bytes at M1 clock %u Hz, flash baseline 0x%08X\n",
           original_clk / 1000000, st.size_bytes, applied_m1_clock_hz(), flash_reference);

    // Static: comfortably bigger than any stack default, and the sweep tables
// don't need per-call state anyway.
    static uint32_t sys_clocks[kMaxSweepClocks];
    const size_t nclk = enumerate_sys_clocks(cfg.sys_clock_min_hz, cfg.sys_clock_max_hz, cfg.clk_sys_step_hz, sys_clocks, kMaxSweepClocks);
    if (nclk == 0) {
        printf("no attainable PLL clock in the sweep range\n");
        for (;;) tight_loop_contents();
    }

    static SweepEntry sweep[kMaxSweepClocks * 7];
    const size_t n = build_sweep(sys_clocks, nclk, cfg.min_m1_divisor, cfg.max_m1_divisor, cfg.m1_min_hz, cfg.m1_step_hz, sweep, kMaxSweepClocks * 7);

    volatile uint32_t* const psram = reinterpret_cast<volatile uint32_t*>(kPsramBase);
    const size_t psram_words = st.size_bytes / sizeof(uint32_t);

    SweepEntry best{}; // last fully-passing entry (max stable M1)
    static SweepEntry passed[kMaxSweepClocks * 7];
    size_t npassed = 0;
    uint32_t current_clk = clock_get_hz(clk_sys);

    printf("\n--- QSPI sweep ---\n");
    for (size_t i = 0; i < n; ++i) {
        const SweepEntry& e = sweep[i];
        // Most steps are a pure M1-divisor change; clk_sys only moves when
        // the next entry needs it. USB stdio rides the PLL_USB-derived 48MHz
        // clock, which set_sys_clock_khz() never touches, so the console
        // needs no re-init on either path -- and re-init is actively
        // harmful: every call re-runs tud_init() on an already-enumerated
        // CDC device, and a handful of those in a row wedges the USB link.
        if (e.sys_clk_hz != current_clk) {
            if (!set_sys_clock_khz(e.sys_clk_hz / 1000, false)) {
                printf("[sys ");
                print_mhz(e.sys_clk_hz);
                printf("] not attainable by PLL -- aborting sweep\n");
                break;
            }
            current_clk = e.sys_clk_hz;
        }
        // Pin the divisor: requesting exactly clk_sys/divisor makes the
        // SDK's ceil() land on it, so M1 = clk_sys/divisor as ordered.
        if (!psram_set_clock_hz(e.sys_clk_hz / e.divisor)) {
            printf("[sys ");
            print_mhz(e.sys_clk_hz);
            printf("] PSRAM M1 retune failed -- aborting sweep\n");
            break;
        }
        const uint32_t m1_clk = applied_m1_clock_hz();

        const bool flash_ok = flash_checksum(cfg.flash_check_bytes) == flash_reference;
        if (!flash_ok) {
            printf("[sys ");
            print_mhz(e.sys_clk_hz);
            printf(" / M1 ");
            print_mhz(m1_clk);
            printf("] FAIL  flash (M0) reads corrupt\n");
        } else {
            PsramFail fail;
            if (!psram_verify(psram, psram_words, fail)) {
                printf("[sys ");
                print_mhz(e.sys_clk_hz);
                printf(" / M1 ");
                print_mhz(m1_clk);
                printf("] FAIL  psram (M1) word %zu expected 0x%08X got 0x%08X\n",
                       fail.offset, fail.expected, fail.actual);
            } else {
                best = e;
                best.m1_clk_hz = m1_clk;
                if (npassed < kMaxSweepClocks * 7)
                    passed[npassed++] = best;
                printf("[sys ");
                print_mhz(e.sys_clk_hz);
                printf(" / M1 ");
                print_mhz(m1_clk);
                printf("] PASS\n");
                continue;
            }
        }
        // The sweep climbs M1, so anything faster than this failure is only
        // more likely to corrupt too -- stop here and restore below.
        break;
    }

    if (npassed == 0) {
        printf("\nno stable M1 clock found -- check wiring/voltage\n");
        set_sys_clock_khz(original_clk / 1000, true);
        psram_set_clock_hz(30'000'000); // conservative boot-style M1
        for (;;) tight_loop_contents();
    }

    // Safe QSPI clock. Two cases:
    //  - The sweep hit a failure: back off by the margin from the max stable M1.
    //  - Every step passed (sweep ran to its ceiling): the limit lies above
    //    the sweep, so there is nothing to back off from -- a margin here
    //    would silently under-report a bus speed the hardware actually holds.
    uint32_t safe_m1 = best.m1_clk_hz;
    const bool limit_not_found = best.m1_clk_hz >= sweep[n - 1].m1_clk_hz; // fastest tested M1 passed
    if (!limit_not_found && best.m1_clk_hz > cfg.margin_hz) {
        const uint32_t want = best.m1_clk_hz - cfg.margin_hz;
        safe_m1 = passed[0].m1_clk_hz;
        for (size_t i = 1; i < npassed; ++i) {
            if (passed[i].m1_clk_hz <= want)
                safe_m1 = passed[i].m1_clk_hz;
            else
                break;
        }
    }
    // The entry that achieved it; ties keep the lower clk_sys (same M1 bus
    // speed, gentler on the CPU and the flash M0 window).
    SweepEntry final_e = passed[0];
    for (size_t i = 1; i < npassed; ++i) {
        if (passed[i].m1_clk_hz > final_e.m1_clk_hz && passed[i].m1_clk_hz <= safe_m1)
            final_e = passed[i];
    }

    // Move to the safe QSPI setting and re-verify it before leaving the
    // system there, so the example never idles at an untested configuration.
    if (final_e.sys_clk_hz != clock_get_hz(clk_sys))
        set_sys_clock_khz(final_e.sys_clk_hz / 1000, true);
    psram_set_clock_hz(final_e.sys_clk_hz / final_e.divisor);
    const bool final_flash_ok = flash_checksum(cfg.flash_check_bytes) == flash_reference;
    PsramFail final_fail;
    const bool final_psram_ok = psram_verify(psram, psram_words, final_fail);

    printf("\n--- result ---\n");
    printf("boot clk_sys was ");
    print_mhz(original_clk);
    printf("\n");
    printf("max stable QSPI (M1) = ");
    print_mhz(best.m1_clk_hz);
    printf("   at clk_sys ");
    print_mhz(best.sys_clk_hz);
    printf(" / divisor %u\n", best.divisor);
    printf("flash (M0) then ran at ");
    print_mhz(best.sys_clk_hz / 2);
    printf(" (the boot-time clk_sys/2 divider)\n");
    printf("safe QSPI           = ");
    print_mhz(safe_m1);
    printf("   at clk_sys ");
    print_mhz(final_e.sys_clk_hz);
    printf(" / divisor %u (margin -%u MHz on M1)\n", final_e.divisor, cfg.margin_hz / 1000000);
    printf("final check at safe setting: flash %s, psram %s\n",
           final_flash_ok ? "PASS" : "FAIL", final_psram_ok ? "PASS" : "FAIL");

    for (;;) tight_loop_contents();
}