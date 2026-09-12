// DVI/HDMI video example (doubles as a build/link smoke test).
//
// UNLIKE this toolset's other examples, this one has not itself been run on
// real hardware -- it's a minimal illustration of dvi.h's own documented
// per-scanline "scanbuf" worker API (dvi_scanbuf_main_16bpp(), see dvi.h),
// written directly from that header and dvi.c's implementation of it, not
// extracted from a working project.
//
// The pin config below (pico_sock_cfg, common_dvi_pin_configs.h) is the
// Waveshare RP2350-PiZero's onboard TMDS connector -- adapt it (or pick a
// different constant from that header) for your own board.
#include "dvi.h"
#include "common_dvi_pin_configs.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"

#include <cstdio>
#include <cstring>

namespace {

dvi_inst g_dvi;

constexpr int kWidth = 640;
// A handful of scanline buffers, recycled forever -- avoids needing a full
// 640x480x16bpp framebuffer (600KB, more than an RP2040/RP2350's on-chip
// SRAM), the same reason real-hardware consumers of this library stream
// from PSRAM instead
// of holding a framebuffer of that size on-chip. dvi_scanbuf_main_16bpp()
// doesn't guarantee which physical scanline consumes which buffer, so every
// buffer here holds an IDENTICAL copy of the same vertical-bar pattern --
// the picture is correct regardless of scan order.
constexpr int kNumBuffers = 4;
uint16_t g_scanlines[kNumBuffers][kWidth];

void fill_test_pattern() {
    // RGB565 vertical colour bars: same pattern in every buffer (see above).
    constexpr uint16_t colours[] = {
        0xF800, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0xF81F, 0xFFFF, 0x0000,
    };
    constexpr int kNumBars = sizeof(colours) / sizeof(colours[0]);
    constexpr int kBarWidth = kWidth / kNumBars;
    uint16_t pattern[kWidth];
    for (int x = 0; x < kWidth; ++x) pattern[x] = colours[x / kBarWidth];
    for (auto& buf : g_scanlines) std::memcpy(buf, pattern, sizeof(pattern));
}

// Core1: dvi_scanbuf_main_16bpp() never returns -- it pops a scanline
// pointer from q_colour_valid, TMDS-encodes it, and pushes the same pointer
// back onto q_colour_free once done. Core0 (below) keeps q_colour_valid fed.
void __not_in_flash_func(core1_main)() {
    dvi_register_irqs_this_core(&g_dvi, DMA_IRQ_0);
    dvi_start(&g_dvi);
    dvi_scanbuf_main_16bpp(&g_dvi);
}

} // namespace

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("DVI/HDMI example: vertical colour bars, 640x480p60\n");

    fill_test_pattern();

    g_dvi.timing = &dvi_timing_640x480p_60hz;
    g_dvi.ser_cfg = pico_sock_cfg; // adapt for your own board -- see common_dvi_pin_configs.h
    // Only needed if your board's TMDS pins are >= 32 (RP2350B's PIO blocks
    // see one 32-pin window at a time, 0-31 or 16-47) -- see
    // the real-hardware finding behind this call. Omit it if every pin in your ser_cfg is < 32.
    pio_set_gpio_base(g_dvi.ser_cfg.pio, 16);
    dvi_init(&g_dvi, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    // Prime the queue: hand every scanline buffer to the library up front,
    // already filled -- the picture never changes again after this.
    for (auto& buf : g_scanlines) {
        void* p = &buf;
        queue_add_blocking_u32(&g_dvi.q_colour_valid, &p);
    }

    multicore_launch_core1(core1_main);

    // Recycle buffers forever: pop whatever the encoder just finished with
    // and hand it straight back (same content, never redrawn).
    while (true) {
        void* p;
        queue_remove_blocking_u32(&g_dvi.q_colour_free, &p);
        queue_add_blocking_u32(&g_dvi.q_colour_valid, &p);
    }
}
