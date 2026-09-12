/*
 * See audio_ring.h's doc comment for the design and the frank-hdmi-audio
 * lineage this follows (with the full=true-only fix applied).
 */
#include "audio_ring.h"

void audio_ring_set(audio_ring_t *ring, audio_sample_t *buffer, uint32_t size) {
    ring->buffer = buffer;
    ring->size = size;
    ring->read = 0;
    ring->write = 0;
}

uint32_t PICO_TOOLSET_AUDIO_RING_FUNC(audio_ring_get_write_size)(const audio_ring_t *ring) {
    uint32_t rp = ring->read;
    uint32_t wp = ring->write;
    if (wp < rp) {
        return rp - wp - 1;
    }
    return ring->size - wp + rp - 1;
}

uint32_t PICO_TOOLSET_AUDIO_RING_FUNC(audio_ring_get_read_size)(const audio_ring_t *ring) {
    uint32_t rp = ring->read;
    uint32_t wp = ring->write;
    if (wp < rp) {
        return ring->size - rp + wp;
    }
    return wp - rp;
}

audio_sample_t *audio_ring_get_buffer(audio_ring_t *ring) { return ring->buffer; }

uint32_t audio_ring_get_write_offset(const audio_ring_t *ring) { return ring->write; }

uint32_t audio_ring_get_read_offset(const audio_ring_t *ring) { return ring->read; }

void audio_ring_advance_write(audio_ring_t *ring, uint32_t n) {
    ring->write = (ring->write + n) & (ring->size - 1);
    PICO_TOOLSET_AUDIO_RING_BARRIER();
}

void PICO_TOOLSET_AUDIO_RING_FUNC(audio_ring_advance_read)(audio_ring_t *ring, uint32_t n) {
    ring->read = (ring->read + n) & (ring->size - 1);
    PICO_TOOLSET_AUDIO_RING_BARRIER();
}

void audio_ring_set_write_offset(audio_ring_t *ring, uint32_t v) {
    ring->write = v;
    PICO_TOOLSET_AUDIO_RING_BARRIER();
}

void audio_ring_set_read_offset(audio_ring_t *ring, uint32_t v) {
    ring->read = v;
    PICO_TOOLSET_AUDIO_RING_BARRIER();
}
