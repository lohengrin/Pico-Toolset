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

// How pixels get from the application to the panel.
enum class St7701Format : uint8_t {
    // width*height RGB565 pixels in SRAM (st7701_encode()d byte order):
    // 115 KB at 240x240, 450 KB at 480x480.
    Rgb565,
    // width*height bytes in SRAM, each an index into a 256-entry colour table
    // expanded by a third PIO state machine: 56 KB / 225 KB. Uses 18 data
    // pins (d0..d0+17) and 4 DMA channels. At width 480 the pixel clock
    // divider is forced to >= 8.
    Palette8,
    // No framebuffer: an application callback renders each line into a small
    // ring of SRAM line buffers just ahead of the beam (RGB565, like Rgb565).
    // Needs scanline_task() running on some core, see St7701::scanline_task().
    Scanline565,
};

// Renders panel line `y` (0..height-1) into `line` (`width` pixels,
// st7701_encode()d). Called from scanline_task() in thread context, not an
// interrupt, but under a deadline of roughly one line time (~20-30 us): keep
// it short and run it (and the data it reads) from RAM, not flash/PSRAM.
using St7701LineCallback = void (*)(int y, uint16_t* line, void* user);

// All fields are board wiring -- no defaults except the behavioral ones at
// the end; see st7701_configs.h.
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
    void*    framebuffer;    // Rgb565: width*height uint16_t; Palette8: width*height uint8_t;
                             // Scanline565: unused. Caller-owned, in SRAM (NOT PSRAM), 4-byte aligned

    St7701Format format = St7701Format::Rgb565;
    St7701LineCallback line_callback = nullptr; // Scanline565 only
    void*    line_user = nullptr;
    // Upper bound for the dot clock (the divider is rounded up from clk_sys, so
    // the real clock lands at or below this; 0 = 34 MHz, the panel's limit in
    // Pimoroni's firmware). Lowering it lengthens every line and slows the
    // refresh rate, giving a Scanline565 callback more time per line:
    // ~25 MHz is a line time of ~21 us at 480 wide, 16 MHz ~33 us.
    uint32_t max_pixel_clock_hz = 0;
    // Source lines actually backed by pixels (0 = `height`). Lines from here
    // down are scanned out as a black band from a single static line, so the
    // framebuffer is width*visible_height and Scanline565 never calls the
    // line callback past it. Palette8: the band shows palette entry
    // `blank_index` (so it follows that entry's colour, black unless the
    // application recolours it).
    uint16_t visible_height = 0;
    uint8_t  blank_index = 0;
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

    [[nodiscard]] uint16_t* framebuffer() const { return static_cast<uint16_t*>(m_cfg.framebuffer); }
    [[nodiscard]] uint8_t* framebuffer8() const { return static_cast<uint8_t*>(m_cfg.framebuffer); }
    [[nodiscard]] St7701Format format() const { return m_cfg.format; }
    [[nodiscard]] int width() const { return m_cfg.width; }
    // Lines backed by pixels (visible_height, or the full height).
    [[nodiscard]] int height() const { return m_visible_height; }

    // Copies a full width*height frame (st7701_encode()d pixels) into the
    // framebuffer behind the scan position so no tearing is visible. No-op if
    // src is the framebuffer itself.
    void present(const uint16_t* src);
    // Palette8: same for a width*height byte frame (waits for vblank first).
    void present8(const uint8_t* src);

    // Palette8: sets colour table entry `index` (any 0-255 RGB; the panel
    // bus carries 5-6-5 bits plus one extra red bit). The table starts as RGB332.
    void set_palette_colour(uint8_t index, uint8_t r, uint8_t g, uint8_t b);

    // Scanline565: renders lines ahead of the beam and arms each frame. Call
    // it in a loop on the core that owns the scanout interrupts (init()'s core,
    // e.g. `while (true) lcd.scanline_task();` on core1); each call renders
    // whatever is due and returns. No-op in the other formats.
    void scanline_task();
    // Frames skipped / lines not ready in time since init (Scanline565).
    [[nodiscard]] uint32_t missed_frames() const { return m_missed_frames; }
    [[nodiscard]] uint32_t late_lines() const { return m_late_lines; }

    // Panel row the beam is on (0..479; stays at 479+ through the vertical porches until the
    // next frame starts and it drops back to 0). With `visible_height` below the panel height,
    // rows >= visible_height read nothing from the application's buffer, so that is the window
    // in which the buffer can be rewritten without a visible seam: a top-to-bottom copy that
    // starts there stays ahead of the beam of the next frame. Updated by the scanout interrupt.
    [[nodiscard]] int display_row() const { return *reinterpret_cast<const volatile int*>(&m_display_row); }

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

    static constexpr int kLineRing = 4;
    static constexpr int kLineLookahead = 2;
    static constexpr int kMaxWidth = 480;

    uint m_parallel_sm = 0, m_timing_sm = 0, m_palette_sm = 0;
    uint m_parallel_offset = 0, m_timing_offset = 0, m_palette_offset = 0;
    int m_dma = -1, m_dma2 = -1, m_dma3 = -1, m_dma4 = -1;

    // Scanline565 state, shared between the line interrupt and scanline_task().
    uint16_t m_line_ring[kLineRing][kMaxWidth];
    volatile int m_scan_src = 0;      // source line the beam is on / next to be fetched
    volatile int m_rendered_upto = -1; // highest source line rendered this frame
    volatile bool m_frame_armed = false;
    volatile uint32_t m_frame_gen = 0; // bumped at every frame start
    bool m_seen_frame = false;
    volatile uint32_t m_missed_frames = 0;
    volatile uint32_t m_late_lines = 0;

    uint16_t* volatile m_next_line_addr = nullptr; // also read by DMA (re-arm channel)
    int m_display_row = 0;
    int m_row_shift = 0;
    int m_visible_height = 0;
    uint16_t m_timing_row = 0;
    uint16_t m_timing_phase = 0;
    volatile bool m_waiting_for_vsync = false;
};

} // namespace pico_toolset

#endif // PICO_RP2350
