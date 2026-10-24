#include "pico_toolset/sram.h"

#include <malloc.h>
#include <unistd.h>

#include "hardware/address_mapped.h"

// Linker-provided (pico-sdk memmap).
extern "C" char __StackLimit;
extern "C" char __bss_end__;

namespace pico_toolset {

size_t sram_heap_total() {
    return static_cast<size_t>(&__StackLimit - &__bss_end__);
}

size_t sram_free_estimate() {
    const size_t total = sram_heap_total();
    const size_t used = static_cast<size_t>(mallinfo().uordblks);
    return total > used ? total - used : 0;
}

size_t sram_contiguous_free() {
    const char* brk = static_cast<const char*>(sbrk(0));
    return brk < &__StackLimit ? static_cast<size_t>(&__StackLimit - brk) : 0;
}

bool in_sram(const void* p) {
    const uintptr_t a = reinterpret_cast<uintptr_t>(p);
    return a >= SRAM_BASE && a < SRAM_END;
}

} // namespace pico_toolset
