/*
 * Lock-free single-producer/single-consumer ring of stereo int16 audio
 * frames, feeding libdvi's HDMI data-island audio packetiser (see
 * data_packet.h). The producer (this project's core0 emulation loop) owns
 * the write index; the consumer (dvi.c's per-scanline DMA IRQ, on core1)
 * owns the read index -- no locking needed as long as each side only ever
 * touches its own index.
 *
 * New file, not a vendored one -- this project's HDMI-audio addition
 * (see README.md and CMakeLists.txt's "PATCH
 * (hdmi-audio):" notes). Design follows rh1tech/frank-hdmi-audio's
 * frank_audio_ring.{c,h} (BSD-3-Clause, itself layered on Wren6991/PicoDVI),
 * with one deliberate behavior change: frank's own `get_write_size()`/
 * `get_read_size()` take a `full` bool whose `false` branch has a documented
 * sign-error that over-reports free space and corrupts in-flight samples
 * (see frank-hdmi-audio's docs/LLM_GUIDE.md, "Audio plays but sounds
 * broken"). This port only ever implements the correct (`full=true`)
 * arithmetic -- there is no unsafe mode to accidentally call.
 *
 * Plain C, not a C++ class: `struct dvi_inst` (dvi.h, patched to add HDMI
 * audio) embeds one of these directly, and dvi.c is compiled as C -- this
 * header must stay valid C so dvi.c can declare that member by including it,
 * while remaining includable from this project's C++ glue/tests too (hence
 * the extern "C" wrapper, matching dvi.h's own convention).
 */
#ifndef PICO_TOOLSET_AUDIO_RING_H
#define PICO_TOOLSET_AUDIO_RING_H

#include <stdint.h>

// __not_in_flash_func()/hardware/sync.h's __dmb() only exist when actually
// compiling for RP2040/RP2350 (PICO_ON_DEVICE, pulled in transitively via
// pico_stdlib) -- never for the native/wasm host builds that also compile
// this file for ctest. See src/core/component/Cpu6809E.cpp's identical
// gating for the established convention this follows.
#if defined(PICO_ON_DEVICE)
#include "hardware/sync.h"
#include "pico.h"
#define PICO_TOOLSET_AUDIO_RING_FUNC(name) __not_in_flash_func(name)
#define PICO_TOOLSET_AUDIO_RING_BARRIER() __dmb()
#else
#define PICO_TOOLSET_AUDIO_RING_FUNC(name) name
// Host builds are single-threaded test code exercising the ring's index
// arithmetic directly, not real cross-core concurrency -- a compiler-only
// memory barrier (preventing instruction reordering across it) is enough to
// keep the read-then-write-index ordering the real device relies on, without
// pulling in a hardware intrinsic that doesn't exist on x86_64/wasm.
#if defined(__GNUC__) || defined(__clang__)
#define PICO_TOOLSET_AUDIO_RING_BARRIER() __asm__ __volatile__("" ::: "memory")
#else
#define PICO_TOOLSET_AUDIO_RING_BARRIER() ((void)0)
#endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct audio_sample {
    int16_t channels[2]; // L, R
} audio_sample_t;

typedef struct audio_ring {
    audio_sample_t *buffer;
    uint32_t size; // power of two; one slot always reserved (full vs empty)
    volatile uint32_t read;
    volatile uint32_t write;
} audio_ring_t;

// Attaches `buffer` (capacity `size`, a power of two > 1) to the ring and
// resets both indices to zero. Non-power-of-two sizes silently corrupt the
// wraparound arithmetic (every index update masks with `size - 1`) -- the
// caller is responsible for sizing correctly, same contract as frank's own
// audio_ring_set().
void audio_ring_set(audio_ring_t *ring, audio_sample_t *buffer, uint32_t size);

// Frames the producer can safely write right now without overtaking the
// consumer (size - 1 usable slots when fully empty -- one slot is always
// reserved to disambiguate full from empty).
uint32_t PICO_TOOLSET_AUDIO_RING_FUNC(audio_ring_get_write_size)(const audio_ring_t *ring);

// Frames available for the consumer to read right now.
uint32_t PICO_TOOLSET_AUDIO_RING_FUNC(audio_ring_get_read_size)(const audio_ring_t *ring);

// Raw pointer to the ring's backing storage (index 0), for callers that want
// to index by an offset returned from get_write_offset()/get_read_offset()
// with their own wraparound math (matching dvi_update_data_packet_()'s
// per-sample consumption pattern in dvi.c).
audio_sample_t *audio_ring_get_buffer(audio_ring_t *ring);

uint32_t audio_ring_get_write_offset(const audio_ring_t *ring);
uint32_t audio_ring_get_read_offset(const audio_ring_t *ring);

// Advances the write/read index by `n` frames (wrapping modulo `size`) and
// issues the barrier that makes the index update visible to the other core
// only after every sample write/read it's guarding has completed.
//
// advance_read() is RAM-resident: it runs every scanline on core1 inside the
// DVI DMA IRQ (via data_packet_set_audio_sample()), where a flash fetch can
// queue behind core0's QMI traffic and blow the scanline deadline -- see
// data_packet.h's "RAM residency" note. advance_write() is core0-only
// (queue_audio_samples()) and has no such constraint.
void audio_ring_advance_write(audio_ring_t *ring, uint32_t n);
void PICO_TOOLSET_AUDIO_RING_FUNC(audio_ring_advance_read)(audio_ring_t *ring, uint32_t n);

// Sets the write/read index directly (wrapping modulo `size` is the
// caller's responsibility -- used once, at HDMI-audio init, to half-fill the
// ring so the rate-matched producer/consumer pair starts in the middle of
// the underrun/overflow window instead of right at empty; see
// dvi_audio_init()'s own doc comment for why that matters).
void audio_ring_set_write_offset(audio_ring_t *ring, uint32_t v);
void audio_ring_set_read_offset(audio_ring_t *ring, uint32_t v);

#ifdef __cplusplus
}
#endif

#endif /* PICO_TOOLSET_AUDIO_RING_H */
