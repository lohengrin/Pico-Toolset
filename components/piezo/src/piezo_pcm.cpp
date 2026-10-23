#include "pico_toolset/piezo_pcm.h"

#if PICO_RP2350

#include <algorithm>
#include <cmath>

#include "hardware/clocks.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/time.h"

namespace pico_toolset {

namespace {

constexpr uint32_t kMask = PiezoPcm::kRingSamples - 1;
constexpr uint16_t kMid = 128;          // idle level while running (duty 128/256)
constexpr uint32_t kResyncLead = 768;   // after an underrun/at start the producer is this far ahead (~17 ms, a bit under one 20 ms frame)

// The DMA ring read needs the ring aligned to its own byte size.
alignas(PiezoPcm::kRingSamples * 2) uint16_t g_ring[PiezoPcm::kRingSamples];

repeating_timer_t g_guard_timer;
PiezoPcm* g_inst = nullptr;

bool guard_cb(repeating_timer_t*) {
    if (g_inst) g_inst->guard();
    return true;
}

} // namespace

bool PiezoPcm::init(const PiezoPcmConfig& config) {
    if (m_inited) return true;
    m_cfg = config;

    gpio_set_function(m_cfg.pin, GPIO_FUNC_PWM);
    m_slice = static_cast<uint8_t>(pwm_gpio_to_slice_num(m_cfg.pin));
    const uint chan = pwm_gpio_to_channel(m_cfg.pin);

    pwm_config pc = pwm_get_default_config();
    pwm_config_set_wrap(&pc, 255);
    pwm_config_set_clkdiv(&pc, 1.0f);
    pwm_init(m_slice, &pc, false);
    pwm_set_gpio_level(m_cfg.pin, 0);

    m_dma = dma_claim_unused_channel(false);
    if (m_dma < 0) return false;
    int timer = -1;
    for (uint t = 0; t < NUM_DMA_TIMERS; ++t) {
        if (!dma_timer_is_claimed(t)) {
            timer = static_cast<int>(t);
            break;
        }
    }
    if (timer < 0) {
        dma_channel_unclaim(static_cast<uint>(m_dma));
        m_dma = -1;
        return false;
    }
    dma_timer_claim(static_cast<uint>(timer));

    // rate = clk_sys * num / den, both 16 bit: the (num, den) with den <= 65535 closest to the target.
    const double sys = static_cast<double>(clock_get_hz(clk_sys));
    const double ratio = static_cast<double>(m_cfg.sample_rate_hz) / sys;
    uint16_t best_num = 1, best_den = static_cast<uint16_t>(std::min(65535.0, std::round(1.0 / ratio)));
    double best_err = 1e9;
    for (uint32_t num = 1; num < 65536; ++num) {
        const double den = std::round(static_cast<double>(num) / ratio);
        if (den > 65535.0) break;
        const double err = std::fabs(static_cast<double>(num) / den - ratio) / ratio;
        if (err < best_err) {
            best_err = err;
            best_num = static_cast<uint16_t>(num);
            best_den = static_cast<uint16_t>(den);
        }
    }
    dma_timer_set_fraction(static_cast<uint>(timer), best_num, best_den);

    // 16-bit stores into the low (channel A) or high (channel B) halfword of the slice's CC register.
    volatile uint16_t* dest = reinterpret_cast<volatile uint16_t*>(&pwm_hw->slice[m_slice].cc) + chan;
    dma_channel_config dc = dma_channel_get_default_config(static_cast<uint>(m_dma));
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_16);
    channel_config_set_read_increment(&dc, true);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_ring(&dc, false, 12); // 2^12 bytes = kRingSamples * 2
    channel_config_set_dreq(&dc, dma_get_timer_dreq(static_cast<uint>(timer)));
    dma_channel_configure(static_cast<uint>(m_dma), &dc, const_cast<uint16_t*>(dest), g_ring,
                          dma_encode_endless_transfer_count(), false);

    std::fill(std::begin(g_ring), std::end(g_ring), kMid);
    m_write_idx = 0;
    m_last_read_idx = 0;
    m_read_total = m_written_total = 0;
    g_inst = this;
    add_repeating_timer_ms(-10, guard_cb, nullptr, &g_guard_timer);
    m_inited = true;
    m_paused = true;
    return true;
}

void PiezoPcm::update_read_position() {
    const uint32_t idx = static_cast<uint32_t>((dma_hw->ch[m_dma].read_addr - reinterpret_cast<uintptr_t>(g_ring)) >> 1) & kMask;
    m_read_total += (idx - m_last_read_idx) & kMask;
    m_last_read_idx = idx;
}

void PiezoPcm::silence_ring() {
    std::fill(std::begin(g_ring), std::end(g_ring), kMid);
}

size_t PiezoPcm::queued() const {
    const int64_t q = static_cast<int64_t>(m_written_total) - static_cast<int64_t>(m_read_total);
    return q > 0 ? static_cast<size_t>(q) : 0;
}

size_t PiezoPcm::free_space() const {
    const size_t q = queued();
    return kRingSamples - 1 > q ? kRingSamples - 1 - q : 0;
}

void PiezoPcm::guard() {
    if (!m_inited || m_paused || m_in_write) return;
    update_read_position();
    if (m_read_total > m_written_total) {
        // The DMA has overtaken the producer: it is replaying samples that were
        // already played. Cover the whole ring with silence and restart the
        // producer a little ahead of the read pointer.
        silence_ring();
        m_underruns = m_underruns + 1;
        m_write_idx = (m_last_read_idx + kResyncLead) & kMask;
        m_written_total = m_read_total + kResyncLead;
    }
}

void PiezoPcm::pause() {
    if (!m_inited || m_paused) return;
    m_paused = true;
    dma_channel_abort(static_cast<uint>(m_dma));
    pwm_set_enabled(m_slice, false);
    pwm_set_gpio_level(m_cfg.pin, 0);
}

void PiezoPcm::resume() {
    if (!m_inited || !m_paused) return;
    m_in_write = true;
    silence_ring();
    m_write_idx = kResyncLead;
    m_last_read_idx = 0;
    m_read_total = 0;
    m_written_total = kResyncLead;
    dma_channel_set_read_addr(static_cast<uint>(m_dma), g_ring, false);
    pwm_set_gpio_level(m_cfg.pin, kMid);
    pwm_set_enabled(m_slice, true);
    dma_channel_start(static_cast<uint>(m_dma));
    m_paused = false;
    m_in_write = false;
}

size_t PiezoPcm::write_samples(const uint16_t* duty, size_t count) {
    if (m_paused) resume();
    m_in_write = true;
    update_read_position();
    if (m_read_total > m_written_total) { // underran since the last guard() tick
        silence_ring();
        m_underruns = m_underruns + 1;
        m_write_idx = (m_last_read_idx + kResyncLead) & kMask;
        m_written_total = m_read_total + kResyncLead;
    }
    const size_t n = std::min(count, free_space());
    for (size_t i = 0; i < n; ++i) {
        g_ring[m_write_idx] = duty[i];
        m_write_idx = (m_write_idx + 1) & kMask;
    }
    m_written_total += n;
    m_in_write = false;
    return n;
}

size_t PiezoPcm::write(const int16_t* mono, size_t count) {
    uint16_t chunk[128];
    size_t done = 0;
    while (done < count) {
        const size_t n = std::min<size_t>(count - done, 128);
        for (size_t i = 0; i < n; ++i) {
            const int v = kMid + (mono[done + i] >> 8);
            chunk[i] = static_cast<uint16_t>(std::clamp(v, 0, 255));
        }
        const size_t accepted = write_samples(chunk, n);
        done += accepted;
        if (accepted < n) break; // ring full
    }
    return done;
}

size_t PiezoPcm::write(const float* mono, size_t count) {
    uint16_t chunk[128];
    size_t done = 0;
    while (done < count) {
        const size_t n = std::min<size_t>(count - done, 128);
        for (size_t i = 0; i < n; ++i) {
            const float s = std::clamp(mono[done + i], -1.0f, 1.0f);
            chunk[i] = static_cast<uint16_t>(kMid + static_cast<int>(s * 127.0f));
        }
        const size_t accepted = write_samples(chunk, n);
        done += accepted;
        if (accepted < n) break;
    }
    return done;
}

} // namespace pico_toolset

#endif // PICO_RP2350
