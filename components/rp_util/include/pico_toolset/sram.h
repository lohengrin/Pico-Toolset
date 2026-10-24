#pragma once

#include <cstddef>

namespace pico_toolset {

// SRAM heap headroom for code that wants a big internal buffer and must know whether it fits
// (picking SRAM vs PSRAM without letting malloc() panic -- the SDK's default
// PICO_MALLOC_PANIC=1 turns a failed malloc into a hard stop).

// Total heap arena: the space between the end of .bss and the bottom of the stack.
size_t sram_heap_total();

// Heap arena minus bytes currently allocated (mallinfo). Counts holes inside the heap, so the
// largest single allocation that actually succeeds can be smaller after fragmentation.
size_t sram_free_estimate();

// Contiguous space above the current heap break: an allocation up to this size always
// succeeds when no free hole fits it first. The conservative number after fragmentation.
size_t sram_contiguous_free();

// True if `p` points into on-chip SRAM (as opposed to flash, XIP-mapped PSRAM or a peripheral).
bool in_sram(const void* p);

} // namespace pico_toolset
