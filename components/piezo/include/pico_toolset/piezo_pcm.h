#pragma once

// PCM audio on a PWM pin (RP2350 only): the 8-bit duty cycle of a fast PWM
// carrier follows the samples, fed by a free-running DMA channel paced by a
// DMA timer -- a "PC speaker with a DAC" for a piezo/buzzer behind a
// transistor, with no CPU cost per sample. Quality is what a piezo gives
// (loud midrange, no bass), good enough for beeps, music and sound effects.
//
// One instance only (the ring buffer is static, 4 KB aligned to itself so the
// DMA read pointer can wrap in hardware).
#if PICO_RP2350

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

struct PiezoPcmConfig {
    uint8_t  pin;                      // PWM-capable GPIO (the piezo's transistor drive)
    uint32_t sample_rate_hz = 44100;   // paced by a DMA timer; clk_sys/num*den approximation, error < 0.01 %
};

class PiezoPcm {
public:
    static constexpr size_t kRingSamples = 2048; // 46 ms at 44.1 kHz

    // Claims a DMA channel + a DMA timer, sets the PWM carrier (clk_sys/256,
    // e.g. 586 kHz at 150 MHz) and starts silent (carrier off) -- call
    // write() or resume() to bring the output up. False if no DMA
    // channel/timer is free.
    bool init(const PiezoPcmConfig& config);

    // Queues mono samples (full scale = +-1.0 / +-32767) behind what is
    // already queued and (re)starts the output if it was paused. Returns how
    // many were accepted; the rest is dropped when the ring is full -- never
    // blocks. Call from one context (the producer); the timer callback that
    // guards against underrun is the only other writer and steps aside while
    // write() runs.
    size_t write(const int16_t* mono, size_t count);
    size_t write(const float* mono, size_t count);

    // Samples queued but not played yet / free space for write().
    [[nodiscard]] size_t queued() const;
    [[nodiscard]] size_t free_space() const;

    // Stops the DMA and drives the pin low (no carrier, no idle current).
    // write()/resume() bring it back.
    void pause();
    void resume();
    [[nodiscard]] bool paused() const { return m_paused; }

    // Underruns detected so far (producer stalled long enough for the DMA to
    // overtake it); each is covered by silence instead of replaying old data.
    [[nodiscard]] uint32_t underruns() const { return m_underruns; }

    // Internal: called by the repeating timer.
    void guard();

private:
    size_t write_samples(const uint16_t* duty, size_t count);
    void update_read_position();
    void silence_ring();

    PiezoPcmConfig m_cfg{};
    uint8_t m_slice = 0;
    int m_dma = -1;
    bool m_inited = false;
    volatile bool m_paused = true;
    volatile bool m_in_write = false;
    uint32_t m_last_read_idx = 0;
    uint64_t m_read_total = 0;    // samples consumed by the DMA, unwrapped
    uint64_t m_written_total = 0; // samples queued by write(), unwrapped (+ resync offsets)
    uint32_t m_write_idx = 0;
    volatile uint32_t m_underruns = 0;
};

} // namespace pico_toolset

#endif // PICO_RP2350
