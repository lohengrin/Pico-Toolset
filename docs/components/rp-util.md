# rp_util

Small RP2040/RP2350 helpers that several projects kept re-writing. No dependencies beyond pico-sdk.

| | |
|---|---|
| Target / option | `pico_toolset_rp_util` (always built) |
| Headers | `pico_toolset/irq_lock.h`, `bootsel.h`, `sram.h` |
| Used by | PicoDoom (BOOTSEL mute, audio mixer locking, SRAM-vs-PSRAM buffer choices) |

## `IrqLock` (header-only)

RAII interrupt-off section on the calling core. For state shared with an interrupt handler or an
async-context worker that preempts thread code on the same core (for example Bluetooth stack
callbacks). Keep it to microseconds; it does not protect against the other core.

## `bootsel_pressed()`

Reads the BOOTSEL button at run time. The button shares the flash chip-select, so a read floats
that line for ~20 us with interrupts off. The function lives in RAM, and **nothing may execute from
flash or XIP-mapped PSRAM meanwhile, including the other core** (a core running RAM-only code, such
as a scanout core, is fine). Poll at tens of ms; no debounce. RP2040 and RP2350.

## SRAM headroom (`sram.h`)

For code choosing between an SRAM and a PSRAM buffer without letting `malloc()` panic
(`PICO_MALLOC_PANIC=1` by default): `sram_heap_total()`, `sram_free_estimate()` (arena minus
`mallinfo` used bytes; includes holes), `sram_contiguous_free()` (space above the heap break; the
conservative number once the heap is fragmented) and `in_sram(ptr)`.
