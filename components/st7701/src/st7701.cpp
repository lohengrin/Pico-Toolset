// ST7701S RGB-parallel driver for Pimoroni Presto's 480x480 panel.
//
// Ported from Pimoroni's Presto driver (MIT, (c) 2024 Pimoroni Ltd; mainly
// Michael Bell and Phil Howard), itself inspired by Dmitry GR's demo and the
// ESPHome st7701s component. The PIO programs (st7701_*.pio) are vendored
// unchanged, as is the palette PIO program. The PicoGraphics/MicroPython glue
// is not carried over; the ISR/DMA scanout design, the 8bpp palette pipeline
// and the panel init sequence are. The Scanline565 mode is new here.
#if PICO_RP2350

#include "pico_toolset/st7701.h"

#include <cstring>

#include "hardware/clocks.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/sync.h"

#include "st7701_palette.pio.h"
#include "st7701_parallel.pio.h"
#include "st7701_timing.pio.h"

namespace pico_toolset {

namespace {

St7701* g_inst = nullptr;

// Palette8 colour table. The palette SM turns a pixel index into this
// table's entry address by shifting (table base >> 10), so it must be
// 1 KB aligned.
alignas(1024) uint32_t g_palette[256];

// Scanned out for every line below St7701Config::visible_height (RGB565 zero =
// black; Palette8 fills it with blank_index). 960 bytes cover either format.
alignas(4) uint16_t g_blank_line[480];

constexpr uint32_t kBacklightPwmTop = 6200;

// Panel timing, in lines / pixel clocks.
constexpr int kVPulse = 8;
constexpr int kVBack = 5 + kVPulse;
constexpr int kVDisplay = 480 + kVBack;
constexpr int kVFront = 5 + kVDisplay;
constexpr int kHFront = 4;
constexpr int kHPulse = 16;
constexpr int kHBack = 30;
constexpr int kHDisplay = 480;

// 9th bit of each 9-bit SPI word: 0 = command, 1 = data.
constexpr uint16_t kCmd = 0x000;
constexpr uint16_t kData = 0x100;

// Command1
constexpr uint8_t SWRESET = 0x01, SLPOUT = 0x11, INVON = 0x21, DISPON = 0x29,
                  MADCTL = 0x36, COLMOD = 0x3A, CND2BKxSEL = 0xFF;
// Command2 BK0
constexpr uint8_t PVGAMCTRL = 0xB0, NVGAMCTRL = 0xB1, LNESET = 0xC0, PORCTRL = 0xC1,
                  INVSET = 0xC2, RGBCTRL = 0xC3, SDIR = 0xC7, COLCTRL = 0xCD;
// Command2 BK1
constexpr uint8_t VHRS = 0xB0, VCOMS = 0xB1, VGHSS = 0xB2, TESTCMD = 0xB3, VGLS = 0xB5,
                  PWCTRL1 = 0xB7, PWCTRL2 = 0xB8, PDR1 = 0xC1, PDR2 = 0xC2;

constexpr char kBankOff[5] = {0x77, 0x01, 0x00, 0x00, 0x00};
constexpr char kBank0[5] = {0x77, 0x01, 0x00, 0x00, 0x10};
constexpr char kBank1[5] = {0x77, 0x01, 0x00, 0x00, 0x11};
constexpr char kBank3[5] = {0x77, 0x01, 0x00, 0x00, 0x13};

void __isr __no_inline_not_in_flash_func(timing_isr)() { g_inst->drive_timing(); }
void __isr __no_inline_not_in_flash_func(end_of_line_isr)() { g_inst->handle_end_of_line(); }

} // namespace

// Fed from the timing ISR whenever the timing SM's TX FIFO has room. Each
// word encodes VSYNC/HSYNC levels, a delay and one PIO instruction to run
// after it (a NOP, or an IRQ that wakes the pixel SM / the line-DMA ISR).
void __not_in_flash_func(St7701::drive_timing)() {
    while (!pio_sm_is_tx_fifo_full(m_cfg.pio, m_timing_sm)) {
        uint32_t instr;
        switch (m_timing_phase) {
        case 0: // front porch
            instr = 0x4000B042u;
            if (m_timing_row >= kVPulse) instr |= 0x80000000u;
            instr |= (kHFront - 3) << 16;
            break;
        case 1: // HSYNC
            instr = 0x0000B042u;
            if (m_timing_row >= kVPulse) instr |= 0x80000000u;
            instr |= (kHPulse - 3) << 16;
            break;
        case 2: // back porch; trigger the pixel SM inside the display window
            instr = 0x40000000u;
            if (m_timing_row >= kVPulse) instr |= 0x80000000u;
            if (m_timing_row >= kVBack && m_timing_row < kVDisplay) instr |= 0xD004u;
            else instr |= 0xB042u;
            instr |= (kHBack - 3) << 16;
            break;
        default: // display; IRQ1 at frame end, IRQ0 per line
            instr = 0x40000000u;
            if (m_timing_row == kVDisplay) instr |= 0xD001u;
            else if (m_timing_row >= kVBack - 1 && m_timing_row < kVDisplay) instr |= 0xD000u;
            else instr |= 0xB042u;
            if (m_timing_row >= kVPulse) instr |= 0x80000000u;
            instr |= (kHDisplay - 3) << 16;
            if (++m_timing_row >= kVFront) m_timing_row = 0;
            break;
        }
        pio_sm_put(m_cfg.pio, m_timing_sm, instr);
        m_timing_phase = (m_timing_phase + 1) & 3;
    }
}

void __not_in_flash_func(St7701::handle_end_of_line)() {
    if (m_cfg.pio->irq & 0x2) start_frame_xfer();
    else start_line_xfer();
}

void __not_in_flash_func(St7701::start_line_xfer)() {
    hw_clear_bits(&m_cfg.pio->irq, 0x1);
    ++m_display_row;
    if (m_display_row == 480) {
        m_next_line_addr = nullptr;
        return;
    }
    const int src = m_display_row >> m_row_shift;
    if (src >= m_visible_height) {
        m_next_line_addr = g_blank_line;
        return;
    }
    switch (m_cfg.format) {
    case St7701Format::Palette8:
        m_next_line_addr = reinterpret_cast<uint16_t*>(
            static_cast<uint8_t*>(m_cfg.framebuffer) + m_cfg.width * src);
        break;
    case St7701Format::Scanline565:
        if (m_rendered_upto < src) m_late_lines = m_late_lines + 1;
        m_scan_src = src;
        m_next_line_addr = m_line_ring[src % kLineRing];
        break;
    default:
        m_next_line_addr = &static_cast<uint16_t*>(m_cfg.framebuffer)[m_cfg.width * src];
        break;
    }
}

namespace {

// RP2350-E5: aborting a channel that is chained to another one can re-trigger it before the
// abort completes, so the busy wait never ends. The scanout interrupt on core 1 hung forever
// there (seen as a black screen, mostly while the radio was busy). The SDK-documented workaround
// is to clear the enable bit of the channel and its chain partner before aborting.
void __not_in_flash_func(abort_chained_dma)(int a, int b) {
    hw_clear_bits(&dma_hw->ch[a].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    hw_clear_bits(&dma_hw->ch[b].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    dma_hw->abort = (1u << a) | (1u << b);
    while ((dma_hw->ch[a].ctrl_trig | dma_hw->ch[b].ctrl_trig) & DMA_CH0_CTRL_TRIG_BUSY_BITS) tight_loop_contents();
    hw_set_bits(&dma_hw->ch[a].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
    hw_set_bits(&dma_hw->ch[b].al1_ctrl, DMA_CH0_CTRL_TRIG_EN_BITS);
}

} // namespace

void __not_in_flash_func(St7701::start_frame_xfer)() {
    hw_clear_bits(&m_cfg.pio->irq, 0x2);

    m_next_line_addr = nullptr;
    abort_chained_dma(m_dma, m_dma2);
    pio_sm_set_enabled(m_cfg.pio, m_parallel_sm, false);
    pio_sm_clear_fifos(m_cfg.pio, m_parallel_sm);
    // pio_sm_restart() empties OSR/ISR without a stallable exec: the original
    // exec'd `out null,32`, which latches EXEC_STALLED forever if the TX FIFO
    // is empty (first frame, DMA not armed yet) and freezes this ISR.
    // X/Y survive the restart, so the width loop counter in Y is preserved.
    pio_sm_restart(m_cfg.pio, m_parallel_sm);
    pio_sm_exec(m_cfg.pio, m_parallel_sm, pio_encode_jmp(m_parallel_offset));
    pio_sm_set_enabled(m_cfg.pio, m_parallel_sm, true);
    m_display_row = 0;

    if (m_cfg.format == St7701Format::Scanline565) {
        // The first line is not rendered yet: scanline_task() renders it and
        // starts the DMA during the vertical blanking interval, so the pixel
        // FIFO never holds stale data from the previous frame.
        if (m_seen_frame && !m_frame_armed) m_missed_frames = m_missed_frames + 1;
        m_seen_frame = true;
        m_frame_gen = m_frame_gen + 1;
        m_frame_armed = false;
        m_rendered_upto = -1;
        m_scan_src = 0;
        m_next_line_addr = m_line_ring[0];
    } else {
        m_next_line_addr = static_cast<uint16_t*>(m_cfg.framebuffer);
        dma_channel_set_read_addr(m_dma, m_cfg.framebuffer, true);
    }

    m_waiting_for_vsync = false;
    __sev();
}

void St7701::write_command(uint8_t command, size_t len, const char* data) const {
    static uint16_t words[20];
    gpio_put(m_cfg.pin_spi_cs, 0);
    uint16_t c = kCmd | command;
    spi_write16_blocking(m_cfg.spi, &c, 1);
    if (data) {
        for (size_t i = 0; i < len; ++i) words[i] = kData | static_cast<uint8_t>(data[i]);
        spi_write16_blocking(m_cfg.spi, words, len);
    }
    gpio_put(m_cfg.pin_spi_cs, 1);
}

void St7701::select_bank(const char (&sel)[5]) const {
    write_command(CND2BKxSEL, sizeof(sel), sel);
}

void St7701::apply_rotation() {
    char madctl = 0x00;
    char sdir = 0x00;
    if (m_cfg.rotate_180) {
        madctl = 0x10; // mirror Y
        sdir = 0x04;   // mirror X
    }
    write_command(MADCTL, 1, &madctl);
    select_bank(kBank0);
    write_command(SDIR, 1, &sdir);
    select_bank(kBankOff);
}

void St7701::panel_init() {
    write_command(SWRESET);
    sleep_ms(150);

    select_bank(kBank0);
    write_command(LNESET, 2, "\x3b\x00");  // (59 + 1) * 8 = 480 lines
    write_command(PORCTRL, 2, "\x0d\x02"); // porches: 13 VBP, 2 VFP (do not change)
    write_command(INVSET, 2, "\x31\x01");
    write_command(COLCTRL, 1, "\x08");     // LED polarity reversed
    write_command(PVGAMCTRL, 16, "\x00\x11\x18\x0e\x11\x06\x07\x08\x07\x22\x04\x12\x0f\xaa\x31\x18");
    write_command(NVGAMCTRL, 16, "\x00\x11\x19\x0e\x12\x07\x08\x08\x08\x22\x04\x11\x11\xa9\x32\x18");
    write_command(RGBCTRL, 3, "\x80\x2e\x0e"); // HV mode, H and V back porch + sync

    select_bank(kBank1);
    write_command(VHRS, 1, "\x60");
    write_command(VCOMS, 1, "\x32");
    write_command(VGHSS, 1, "\x07");
    write_command(TESTCMD, 1, "\x80");
    write_command(VGLS, 1, "\x49");
    write_command(PWCTRL1, 1, "\x85");
    write_command(PWCTRL2, 1, "\x21");
    write_command(PDR1, 1, "\x78");
    write_command(PDR2, 1, "\x78");

    // Undocumented sequence specific to the TL040WVS03CT15-H1263A panel; the
    // display stays blank without it (carried over verbatim from Pimoroni).
    write_command(0xE0, 3, "\x00\x1b\x02");
    write_command(0xE1, 11, "\x08\xa0\x00\x00\x07\xa0\x00\x00\x00\x44\x44");
    write_command(0xE2, 12, "\x11\x11\x44\x44\xed\xa0\x00\x00\xec\xa0\x00\x00");
    write_command(0xE3, 4, "\x00\x00\x11\x11");
    write_command(0xE4, 2, "\x44\x44");
    write_command(0xE5, 16, "\x0a\xe9\xd8\xa0\x0c\xeb\xd8\xa0\x0e\xed\xd8\xa0\x10\xef\xd8\xa0");
    write_command(0xE6, 4, "\x00\x00\x11\x11");
    write_command(0xE7, 2, "\x44\x44");
    write_command(0xE8, 16, "\x09\xe8\xd8\xa0\x0b\xea\xd8\xa0\x0d\xec\xd8\xa0\x0f\xee\xd8\xa0");
    write_command(0xEB, 7, "\x02\x00\xe4\xe4\x88\x00\x40");
    write_command(0xEC, 2, "\x3c\x00");
    write_command(0xED, 16, "\xab\x89\x76\x54\x02\xff\xff\xff\xff\xff\xff\x20\x45\x67\x98\xba");
    write_command(MADCTL, 1, "\x00");

    select_bank(kBank3);
    write_command(0xE5, 1, "\xe4");

    select_bank(kBankOff);
    write_command(COLMOD, 1, "\x66"); // 18 bits per pixel on the RGB bus

    apply_rotation();

    write_command(INVON);
    sleep_ms(1);
    write_command(SLPOUT);
    sleep_ms(120);
    write_command(DISPON);
    sleep_ms(50);
}

bool St7701::init(const St7701Config& config) {
    if (m_inited) return true;
    m_cfg = config;
    g_inst = this;
    m_row_shift = (m_cfg.height == 240) ? 1 : 0;
    m_visible_height = (m_cfg.visible_height == 0 || m_cfg.visible_height > m_cfg.height)
                           ? m_cfg.height : m_cfg.visible_height;
    if (m_cfg.format == St7701Format::Palette8)
        std::memset(g_blank_line, m_cfg.blank_index, sizeof(g_blank_line));
    else
        std::memset(g_blank_line, 0, sizeof(g_blank_line));
    const bool palette = m_cfg.format == St7701Format::Palette8;
    const bool scanline = m_cfg.format == St7701Format::Scanline565;
    if (m_cfg.width > kMaxWidth || (scanline && !m_cfg.line_callback) ||
        (!scanline && !m_cfg.framebuffer))
        return false;
    PIO pio = m_cfg.pio;

    // Claim everything up front so a failure can release it cleanly.
    int sms[3] = {-1, -1, -1};
    int dmas[4] = {-1, -1, -1, -1};
    const int num_sms = palette ? 3 : 2;
    const int num_dmas = palette ? 4 : 2;
    bool ok = true;
    for (int i = 0; i < num_sms && ok; ++i) ok = (sms[i] = pio_claim_unused_sm(pio, false)) >= 0;
    for (int i = 0; i < num_dmas && ok; ++i) ok = (dmas[i] = dma_claim_unused_channel(false)) >= 0;
    if (!ok) {
        for (int sm : sms) if (sm >= 0) pio_sm_unclaim(pio, sm);
        for (int d : dmas) if (d >= 0) dma_channel_unclaim(d);
        return false;
    }
    m_parallel_sm = sms[0];
    m_timing_sm = sms[1];
    m_palette_sm = palette ? sms[2] : 0;
    m_dma = dmas[0];
    m_dma2 = dmas[1];
    m_dma3 = palette ? dmas[2] : -1;
    m_dma4 = palette ? dmas[3] : -1;

    m_timing_offset = pio_add_program(pio, &st7701_timing_program);
    if (palette) {
        m_parallel_offset = pio_add_program(pio, &st7701_parallel_18bpp_program);
        m_palette_offset = pio_add_program(pio, &st7701_palette_program);
        for (int i = 0; i < 256; ++i)
            set_palette_colour(i, (i & 0xE0), (i & 0x1C) << 3, (i & 0x03) << 6); // RGB332
    } else {
        m_parallel_offset = pio_add_program(pio, &st7701_parallel_program);
    }

    spi_init(m_cfg.spi, 8'000'000);
    gpio_set_function(m_cfg.pin_spi_cs, GPIO_FUNC_SIO);
    gpio_set_dir(m_cfg.pin_spi_cs, GPIO_OUT);
    gpio_put(m_cfg.pin_spi_cs, 1);
    gpio_set_function(m_cfg.pin_spi_dat, GPIO_FUNC_SPI);
    gpio_set_function(m_cfg.pin_spi_clk, GPIO_FUNC_SPI);
    // 3-line serial interface: 9th bit is D/CX (low = command, high = data)
    spi_set_format(m_cfg.spi, 9, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    pio_gpio_init(pio, m_cfg.pin_hsync);
    pio_gpio_init(pio, m_cfg.pin_vsync);
    pio_gpio_init(pio, m_cfg.pin_de);
    pio_gpio_init(pio, m_cfg.pin_dot_clk);

    const uint kDataPins = palette ? 18 : 16;
    for (uint i = 0; i < kDataPins; ++i) pio_gpio_init(pio, m_cfg.pin_d0 + i);
    for (uint i = kDataPins; i < 18; ++i) {
        gpio_init(m_cfg.pin_d0 + i);
        gpio_set_dir(m_cfg.pin_d0 + i, GPIO_OUT);
        gpio_put(m_cfg.pin_d0 + i, false);
    }

    pio_sm_set_consecutive_pindirs(pio, m_parallel_sm, m_cfg.pin_d0, kDataPins, true);
    pio_sm_set_consecutive_pindirs(pio, m_parallel_sm, m_cfg.pin_hsync, 4, true);

    pio_sm_config c = palette ? st7701_parallel_18bpp_program_get_default_config(m_parallel_offset)
                              : st7701_parallel_program_get_default_config(m_parallel_offset);
    sm_config_set_out_pins(&c, m_cfg.pin_d0, kDataPins);
    sm_config_set_sideset_pins(&c, m_cfg.pin_de);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&c, true, true, 32);
    sm_config_set_in_shift(&c, false, false, 32);

    // Pixel clock is capped at max_pixel_clock_hz (default 34 MHz); the full-res pixel SM runs at twice the
    // timing SM's rate (one pixel per 2 SM cycles vs. two cycles per pixel).
    // Palette mode needs time for the table lookup: divider >= 8 at full width.
    const uint32_t max_pixel_hz = m_cfg.max_pixel_clock_hz ? m_cfg.max_pixel_clock_hz : 34'000'000;
    const uint32_t sys_hz = clock_get_hz(clk_sys);
    uint32_t clk_div = (sys_hz + max_pixel_hz - 1) / max_pixel_hz;
    if (palette && m_cfg.width == 480 && clk_div < 8) clk_div = 8;
    if (m_cfg.width == 480) {
        if (clk_div & 1) clk_div += 1;
        sm_config_set_clkdiv(&c, clk_div >> 1);
    } else {
        sm_config_set_clkdiv(&c, clk_div);
    }

    pio_sm_init(pio, m_parallel_sm, m_parallel_offset, &c);
    pio_sm_exec(pio, m_parallel_sm, pio_encode_out(pio_y, 32));
    pio_sm_put(pio, m_parallel_sm, (m_cfg.width >> 1) - 1);
    pio_sm_set_enabled(pio, m_parallel_sm, true);

    c = st7701_timing_program_get_default_config(m_timing_offset);
    sm_config_set_out_pins(&c, m_cfg.pin_hsync, 2);
    sm_config_set_sideset_pins(&c, m_cfg.pin_dot_clk);
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
    sm_config_set_out_shift(&c, false, true, 32);
    sm_config_set_clkdiv(&c, clk_div);
    pio_sm_init(pio, m_timing_sm, m_timing_offset, &c);
    pio_sm_set_enabled(pio, m_timing_sm, true);

    if (palette) {
        c = st7701_palette_program_get_default_config(m_palette_offset);
        sm_config_set_out_shift(&c, false, true, 32);
        sm_config_set_in_shift(&c, true, true, 30);
        pio_sm_init(pio, m_palette_sm, m_palette_offset, &c);
        pio_sm_exec(pio, m_palette_sm, pio_encode_out(pio_x, 32));
        pio_sm_put(pio, m_palette_sm, reinterpret_cast<uintptr_t>(g_palette) >> 10);
        pio_sm_set_enabled(pio, m_palette_sm, true);
    }

    // Line DMA: line buffer -> pixel SM (RGB565) or palette SM (indices),
    // then chain to a second channel that re-arms the first from
    // m_next_line_addr (set per line by the interrupt).
    dma_channel_config dc = dma_channel_get_default_config(m_dma);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_dreq(&dc, pio_get_dreq(pio, palette ? m_palette_sm : m_parallel_sm, true));
    channel_config_set_bswap(&dc, true);
    channel_config_set_chain_to(&dc, m_dma2);
    dma_channel_configure(m_dma, &dc, &pio->txf[palette ? m_palette_sm : m_parallel_sm], nullptr,
                          palette ? (m_cfg.width >> 2) : (m_cfg.width >> 1), false);

    dc = dma_channel_get_default_config(m_dma2);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
    channel_config_set_read_increment(&dc, false);
    dma_channel_configure(m_dma2, &dc, &dma_hw->ch[m_dma].al3_read_addr_trig,
                          &m_next_line_addr, 1, false);

    if (palette) {
        // The palette SM returns each pixel's table-entry *address*; dma4 moves
        // it into dma3's read address (triggering it), and dma3 copies the
        // 18-bit table entry itself into the pixel SM. They chain forever,
        // paced by the two state machines' DREQs.
        dc = dma_channel_get_default_config(m_dma3);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
        channel_config_set_dreq(&dc, pio_get_dreq(pio, m_parallel_sm, true));
        channel_config_set_read_increment(&dc, false);
        channel_config_set_chain_to(&dc, m_dma4);
        dma_channel_configure(m_dma3, &dc, &pio->txf[m_parallel_sm], nullptr, 1, false);

        dc = dma_channel_get_default_config(m_dma4);
        channel_config_set_transfer_data_size(&dc, DMA_SIZE_32);
        channel_config_set_dreq(&dc, pio_get_dreq(pio, m_palette_sm, false));
        channel_config_set_read_increment(&dc, false);
        dma_channel_configure(m_dma4, &dc, &dma_hw->ch[m_dma3].al3_read_addr_trig,
                              &pio->rxf[m_palette_sm], 1, true);
    }

    m_scan_src = 0;
    m_rendered_upto = -1;
    m_frame_armed = false;
    m_missed_frames = 0;
    m_late_lines = 0;

    if (m_cfg.pin_backlight != 0xFF) {
        pwm_config pc = pwm_get_default_config();
        pwm_config_set_wrap(&pc, kBacklightPwmTop);
        pwm_init(pwm_gpio_to_slice_num(m_cfg.pin_backlight), &pc, true);
        gpio_set_function(m_cfg.pin_backlight, GPIO_FUNC_PWM);
        set_backlight(0); // off until the first frame is scanning out
    }

    panel_init();

    hw_set_bits(&pio->inte1, 0x010u << m_timing_sm); // timing SM TX not full
    irq_set_exclusive_handler(pio_get_irq_num(pio, 1), timing_isr);
    irq_set_priority(pio_get_irq_num(pio, 1), 0x40);
    irq_set_enabled(pio_get_irq_num(pio, 1), true);

    hw_set_bits(&pio->inte0, 0x300); // PIO IRQ flags 0 and 1: line / frame end
    irq_set_exclusive_handler(pio_get_irq_num(pio, 0), end_of_line_isr);
    irq_set_priority(pio_get_irq_num(pio, 0), 0x40);
    irq_set_enabled(pio_get_irq_num(pio, 0), true);

    m_inited = true;

    if (m_cfg.pin_backlight != 0xFF) {
        sleep_ms(50); // let the first frame land before lighting the panel
        set_backlight(255);
    }
    return true;
}

void St7701::cleanup() {
    if (!m_inited) return;
    PIO pio = m_cfg.pio;
    const bool palette = m_cfg.format == St7701Format::Palette8;

    irq_set_enabled(pio_get_irq_num(pio, 0), false);
    irq_remove_handler(pio_get_irq_num(pio, 0), end_of_line_isr);
    irq_set_enabled(pio_get_irq_num(pio, 1), false);
    irq_remove_handler(pio_get_irq_num(pio, 1), timing_isr);
    hw_clear_bits(&pio->inte0, 0x300);
    hw_clear_bits(&pio->inte1, 0x010u << m_timing_sm);

    m_next_line_addr = nullptr;
    if (palette) {
        dma_channel_abort(m_dma4);
        dma_channel_abort(m_dma3);
        dma_channel_unclaim(m_dma4);
        dma_channel_unclaim(m_dma3);
    }
    abort_chained_dma(m_dma, m_dma2);
    dma_channel_unclaim(m_dma);
    dma_channel_unclaim(m_dma2);

    pio_sm_set_enabled(pio, m_parallel_sm, false);
    pio_sm_clear_fifos(pio, m_parallel_sm);
    pio_sm_unclaim(pio, m_parallel_sm);
    pio_sm_set_enabled(pio, m_timing_sm, false);
    pio_sm_clear_fifos(pio, m_timing_sm);
    pio_sm_unclaim(pio, m_timing_sm);
    pio_remove_program(pio, &st7701_timing_program, m_timing_offset);
    if (palette) {
        pio_sm_set_enabled(pio, m_palette_sm, false);
        pio_sm_clear_fifos(pio, m_palette_sm);
        pio_sm_unclaim(pio, m_palette_sm);
        pio_remove_program(pio, &st7701_palette_program, m_palette_offset);
        pio_remove_program(pio, &st7701_parallel_18bpp_program, m_parallel_offset);
    } else {
        pio_remove_program(pio, &st7701_parallel_program, m_parallel_offset);
    }

    m_timing_row = m_timing_phase = 0;
    m_inited = false;
}

void St7701::present(const uint16_t* src) {
    uint16_t* fb = framebuffer();
    if (!m_inited || m_cfg.format != St7701Format::Rgb565 || src == fb) return;

    const int w = m_cfg.width;
    const size_t total = static_cast<size_t>(w) * m_visible_height;
    uint16_t* dst = fb;
    uint16_t* const end = fb + total;
    const uint16_t* s = src;
    volatile uintptr_t* line_ptr = reinterpret_cast<volatile uintptr_t*>(&m_next_line_addr);

    // Copy only the part of the frame that scanout has already passed (or
    // everything once it has wrapped), chasing the beam so no tear is visible.
    while (dst != end) {
        uint16_t* next = reinterpret_cast<uint16_t*>(*line_ptr);
        if (!next || next == fb || next == g_blank_line) next = fb + total;
        next -= w;
        if (next < dst) next = end; // scan is behind the copy: race ahead of it
        if (dst < next) {
            size_t len = next - dst;
            std::memcpy(dst, s, len * sizeof(uint16_t));
            dst += len;
            s += len;
        }
    }
}

void St7701::set_backlight(uint8_t brightness) {
    if (m_cfg.pin_backlight == 0xFF) return;
    uint16_t value;
    if (brightness == 0) value = 0;
    else if (brightness == 255) value = kBacklightPwmTop;
    else value = 181 + (brightness * brightness) / 85;
    pwm_set_gpio_level(m_cfg.pin_backlight, value);
}

void St7701::present8(const uint8_t* src) {
    if (!m_inited || m_cfg.format != St7701Format::Palette8 || src == framebuffer8()) return;
    wait_for_vsync();
    std::memcpy(framebuffer8(), src, static_cast<size_t>(m_cfg.width) * m_visible_height);
}

void St7701::set_palette_colour(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    // Encoding: 18-bit value, RGB565 followed by the low bit of red, MSB
    // aligned (bit reversal for the bus is done by the PIO program).
    g_palette[index] = ((static_cast<uint32_t>(r) << 24) & 0xF8000000u) |
                       ((static_cast<uint32_t>(g) << 19) & 0x07E00000u) |
                       ((static_cast<uint32_t>(b) << 13) & 0x001F8000u) |
                       ((static_cast<uint32_t>(r) << 12) & 0x00004000u);
}

void __not_in_flash_func(St7701::scanline_task)() {
    if (!m_inited || m_cfg.format != St7701Format::Scanline565) return;

    // The frame-start interrupt resets the render bookkeeping; a render that
    // straddles it must not publish its result into the new frame (that left
    // stale lines on screen and flickered the whole picture), so every
    // publish below re-checks the frame generation with interrupts off.
    const uint32_t gen = m_frame_gen;

    if (!m_frame_armed) {
        if (m_display_row != 0) return; // blanking window already over: skip this frame
        m_cfg.line_callback(0, m_line_ring[0], m_cfg.line_user);
        const uint32_t save = save_and_disable_interrupts();
        if (gen == m_frame_gen && m_display_row == 0) {
            m_rendered_upto = 0;
            // Start streaming line 0 now that it is complete.
            dma_channel_set_read_addr(m_dma, m_line_ring[0], true);
            m_frame_armed = true;
        }
        restore_interrupts(save);
        return;
    }

    const int last = m_visible_height - 1; // counts source lines
    while (gen == m_frame_gen) {
        int target = m_scan_src + kLineLookahead;
        if (target > last) target = last;
        const int y = m_rendered_upto + 1;
        if (y > target) break;
        m_cfg.line_callback(y, m_line_ring[y % kLineRing], m_cfg.line_user);
        const uint32_t save = save_and_disable_interrupts();
        if (gen == m_frame_gen) m_rendered_upto = y;
        restore_interrupts(save);
    }
}

void __no_inline_not_in_flash_func(St7701::wait_for_vsync)() {
    m_waiting_for_vsync = true;
    while (m_waiting_for_vsync) __wfe();
}

} // namespace pico_toolset

#endif // PICO_RP2350
