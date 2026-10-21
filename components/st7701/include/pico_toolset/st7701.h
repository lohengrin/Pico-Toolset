#pragma once

// ST7701S RGB-parallel LCD driver (RP2350 only). Unlike the windowed SPI
// panels behind DisplayPanel, this panel is *scanned out continuously* by
// PIO + DMA from a caller-owned RGB565 framebuffer in SRAM, so it has its own
// small API (framebuffer/present/vsync) instead of implementing DisplayPanel.
//
// Ported from Pimoroni's Presto ST7701 driver (MIT, Michael Bell and Phil
// Howard / Pimoroni Ltd), without the PicoGraphics/MicroPython glue.
#if PICO_RP2350

#include <cstddef>
#include <cstdint>

#include "hardware/pio.h"
#include "hardware/spi.h"

namespace pico_toolset {

// All fields are board wiring -- no defaults; see st7701_configs.h.
struct St7701Config {
    uint16_t width;          // 240 (lines/columns doubled by scanout) or 480
    uint16_t height;         // 240 or 480 (== width on the supported panel)
    bool     rotate_180;     // only a 180 degree flip is possible (scanout is raster order)

    uint8_t  pin_d0;         // first of 16 consecutive data pins (RGB565, D0..D15);
                             // the 2 unused low-order lines (d0+16, d0+17) are driven low
    uint8_t  pin_hsync;
    uint8_t  pin_vsync;
    uint8_t  pin_de;
    uint8_t  pin_dot_clk;

    spi_inst_t* spi;         // 9-bit "3-wire" SPI used only for the init sequence
    uint8_t  pin_spi_cs;
    uint8_t  pin_spi_clk;
    uint8_t  pin_spi_dat;
    uint8_t  pin_backlight;  // PWM; 0xFF = none

    PIO      pio;            // owns 2 state machines and the whole IRQ0/IRQ1 pair
    uint16_t* framebuffer;   // width*height pixels, caller-owned, in SRAM (NOT PSRAM)
};

// Pixel format of the framebuffer: RGB565 with the two bytes swapped (the DMA
// swaps them back to the order the PIO program shifts out).
[[nodiscard]] constexpr uint16_t st7701_encode(uint16_t rgb565) {
    return static_cast<uint16_t>((rgb565 << 8) | (rgb565 >> 8));
}

// Only one instance can exist (the PIO interrupt handlers reach it through a
// file-scope pointer). The scanout interrupts are serviced by the core that
// calls init(): call it from core1 to keep scanout independent of core0
// flash/PSRAM stalls (see docs/components/st7701.md).
class St7701 {
public:
    // Brings up the panel (SPI init sequence, then PIO timing/pixel state
    // machines + DMA + interrupts). Blocks ~0.4 s. Returns false if the PIO
    // has no free state machines or DMA channels left.
    bool init(const St7701Config& config);

    // Stops scanout, frees PIO/DMA/IRQs. Safe to call without init().
    void cleanup();

    [[nodiscard]] uint16_t* framebuffer() const { return m_cfg.framebuffer; }
    [[nodiscard]] int width() const { return m_cfg.width; }
    [[nodiscard]] int height() const { return m_cfg.height; }

    // Copies a full width*height frame (st7701_encode()d pixels) into the
    // framebuffer behind the scan position so no tearing is visible. No-op if
    // src is the framebuffer itself.
    void present(const uint16_t* src);

    // Blocks (WFE) until the next vertical blank.
    void wait_for_vsync();

    // 0-255, perceptual curve. No-op without a backlight pin.
    void set_backlight(uint8_t brightness);

    // Interrupt entry points -- not for application use.
    void drive_timing();
    void handle_end_of_line();

private:
    void write_command(uint8_t command, size_t len = 0, const char* data = nullptr) const;
    void select_bank(const char (&sel)[5]) const;
    void panel_init();
    void apply_rotation();
    void start_line_xfer();
    void start_frame_xfer();

    St7701Config m_cfg{};
    bool m_inited = false;

    uint m_parallel_sm = 0, m_timing_sm = 0;
    uint m_parallel_offset = 0, m_timing_offset = 0;
    int m_dma = -1, m_dma2 = -1;

    uint16_t* volatile m_next_line_addr = nullptr; // also read by DMA (re-arm channel)
    int m_display_row = 0;
    int m_row_shift = 0;
    uint16_t m_timing_row = 0;
    uint16_t m_timing_phase = 0;
    volatile bool m_waiting_for_vsync = false;
};

} // namespace pico_toolset

#endif // PICO_RP2350
