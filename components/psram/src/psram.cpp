// PSRAM driver for RP2350 -- built on pico-sdk's hardware_psram driver
// (detect/configure/memory-map via QMI CS1) plus a self-test and a free-list
// allocator over the mapped region. On RP2040 this file is empty.
#if PICO_RP2350

#include "pico_toolset/psram.h"

#include "hardware/clocks.h"
#include "hardware/flash.h"
#include "pico/error.h"

#include <cstdlib>

namespace pico_toolset {

namespace {

// PSRAM (QMI CS1) maps at XIP_BASE + the board's flash size -- fixed origin
// 0x11000000 for the standard 16 MB flash RP2350 boards.
constexpr uintptr_t kPsramBase = 0x11000000;

constexpr uint32_t kPsramMinDeselectNs = 50;

PsramStatus g_status;
bool g_initialized = false;

struct Block {
    size_t size;
    bool free;
    Block* next;
};
Block* g_free_list = nullptr;

constexpr size_t align_up(size_t n, size_t align) { return (n + align - 1) & ~(align - 1); }

bool run_self_test(uint8_t* base, size_t size, PsramStatus& status) {
    for (size_t i = 0; i < status.self_test_samples; ++i) {
        size_t offset = (size / status.self_test_samples) * i;
        volatile uint8_t* p = base + offset;
        uint8_t pattern = static_cast<uint8_t>(offset ^ 0x55);
        *p = pattern;
        uint8_t read_back = *p;
        if (read_back != pattern) {
            status.fail_offset = offset;
            status.fail_expected = pattern;
            status.fail_actual = read_back;
            status.fail_on_second_pattern = false;
            return false;
        }
        uint8_t inverted = static_cast<uint8_t>(~pattern);
        *p = inverted;
        read_back = *p;
        if (read_back != inverted) {
            status.fail_offset = offset;
            status.fail_expected = inverted;
            status.fail_actual = read_back;
            status.fail_on_second_pattern = true;
            return false;
        }
    }
    return true;
}

} // namespace

PsramStatus psram_init(const PsramConfig& config) {
    g_status = PsramStatus{};
    g_status.self_test_samples = config.self_test_samples;
    g_free_list = nullptr;
    g_initialized = false;

    uint8_t cs_pins[] = {config.cs_pin};
    size_t size = psram_detect_cs_and_size(cs_pins, 1);
    if (size == 0) {
        return g_status; // not present (or not responding on this CS pin)
    }
    g_status.present = true;
    g_status.size_bytes = size;

    // psram_detect_size() restores flash_devinfo's CS1 size to its pre-probe
    // value once done; on a fresh boot that's FLASH_DEVINFO_SIZE_NONE, without
    // which psram_reinitialize() would configure a zero-size window -- every
    // access then reads back 0. Restore it from the detected size.
    flash_devinfo_set_cs_size(1, flash_devinfo_bytes_to_size(static_cast<uint32_t>(size)));

    g_status.clk_sys_hz_at_test = clock_get_hz(clk_sys);

    // psram_configure_params() computes divisor = ceil(clk_sys/max_freq) and
    // rxdelay = divisor, but QMI_M1_TIMING_RXDELAY is a 3-bit field (max 7)
    // whose bounds check is compiled out by default -- an out-of-range
    // rxdelay would be silently truncated. Clamp the effective max frequency
    // so the divisor can never exceed 7 whatever clk_sys is.
    constexpr uint32_t kMaxRxdelayDivisor = 7;
    uint32_t min_freq_for_divisor_limit = g_status.clk_sys_hz_at_test / kMaxRxdelayDivisor + 1;
    uint32_t max_freq_hz = config.max_clock_hz;
    if (max_freq_hz == 0)
        max_freq_hz = 30'000'000; // conservative; see TOM6809's bring-up notes
    if (max_freq_hz < min_freq_for_divisor_limit)
        max_freq_hz = min_freq_for_divisor_limit;

    if (psram_configure_params(max_freq_hz, PICO_DEFAULT_PSRAM_MAX_SELECT, kPsramMinDeselectNs) != PICO_OK) {
        return g_status;
    }
    if (psram_reinitialize() != PICO_OK) {
        return g_status;
    }

    auto* base = reinterpret_cast<uint8_t*>(kPsramBase);
    if (config.run_self_test && !run_self_test(base, size, g_status)) {
        return g_status;
    }

    g_status.test_ok = true;
    g_initialized = true;

    g_free_list = reinterpret_cast<Block*>(base);
    g_free_list->size = size - sizeof(Block);
    g_free_list->free = true;
    g_free_list->next = nullptr;
    return g_status;
}

const PsramStatus& psram_status() { return g_status; }

extern "C" void* psram_malloc(size_t size) {
    if (!g_initialized || !g_status.test_ok || size == 0) return nullptr;
    size = align_up(size, 8);

    for (Block* b = g_free_list; b != nullptr; b = b->next) {
        if (!b->free || b->size < size) continue;
        if (b->size >= size + sizeof(Block) + 8) {
            auto* remainder = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(b) + sizeof(Block) + size);
            remainder->size = b->size - size - sizeof(Block);
            remainder->free = true;
            remainder->next = b->next;
            b->next = remainder;
            b->size = size;
        }
        b->free = false;
        return reinterpret_cast<uint8_t*>(b) + sizeof(Block);
    }
    return nullptr; // OOM
}

extern "C" void psram_free(void* ptr) {
    if (!ptr || !g_initialized) return;
    auto* b = reinterpret_cast<Block*>(reinterpret_cast<uint8_t*>(ptr) - sizeof(Block));
    b->free = true;

    // Coalesce adjacent free blocks (list order matches address order).
    for (Block* cur = g_free_list; cur != nullptr && cur->next != nullptr;) {
        bool adjacent =
            reinterpret_cast<uint8_t*>(cur) + sizeof(Block) + cur->size == reinterpret_cast<uint8_t*>(cur->next);
        if (cur->free && cur->next->free && adjacent) {
            cur->size += sizeof(Block) + cur->next->size;
            cur->next = cur->next->next;
        } else {
            cur = cur->next;
        }
    }
}

size_t psram_used_bytes() {
    if (!g_initialized || !g_status.test_ok) return 0;
    size_t free_bytes = 0;
    for (Block* b = g_free_list; b != nullptr; b = b->next)
        if (b->free) free_bytes += b->size;
    return g_status.size_bytes - free_bytes;
}

} // namespace pico_toolset

#endif // PICO_RP2350