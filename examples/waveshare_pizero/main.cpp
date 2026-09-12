// Waveshare RP2350-PiZero base combination example (doubles as an
// integration test): HDMI video-out + PSRAM (works whether or not the chip
// is populated) + uSD card + USB HID host, all together. See
// ../../boards/waveshare_rp2350_pizero.md.
//
// Like components/dvi_hdmi/example/dvi_hdmi_example.cpp, the DVI half has
// not been independently re-run on real hardware from this repo, though its
// pin config (pico_sock_cfg) is the one that example's own header comment
// documents as this board's onboard TMDS connector.
#include "dvi.h"
#include "common_dvi_pin_configs.h"
#include "pico_toolset/psram.h"
#include "pico_toolset/psram_configs.h"
#include "pico_toolset/sdcard.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico_toolset/usb_hid_host.h"
#include "pico_toolset/usb_hid_configs.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"

#include <cstdio>
#include <cstring>

using pico_toolset::SdCard;
using pico_toolset::UsbHidHost;

namespace {

dvi_inst g_dvi;

constexpr int kDviWidth = 640;
constexpr int kDviNumBuffers = 4;
uint16_t g_scanlines[kDviNumBuffers][kDviWidth];

void fill_test_pattern() {
    constexpr uint16_t colours[] = {
        0xF800, 0xFFE0, 0x07E0, 0x07FF, 0x001F, 0xF81F, 0xFFFF, 0x0000,
    };
    constexpr int kNumBars = sizeof(colours) / sizeof(colours[0]);
    constexpr int kBarWidth = kDviWidth / kNumBars;
    uint16_t pattern[kDviWidth];
    for (int x = 0; x < kDviWidth; ++x) pattern[x] = colours[x / kBarWidth];
    for (auto& buf : g_scanlines) std::memcpy(buf, pattern, sizeof(pattern));
}

void __not_in_flash_func(core1_main)() {
    dvi_register_irqs_this_core(&g_dvi, DMA_IRQ_0);
    dvi_start(&g_dvi);
    dvi_scanbuf_main_16bpp(&g_dvi);
}

} // namespace

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("Waveshare RP2350-PiZero example: HDMI + PSRAM + SD + USB HID\n");

    // -- PSRAM: log status either way, don't fail if unpopulated -- this is
    // how one binary covers "with or without PSRAM" (see PsramStatus's doc
    // comment: present=false is a normal, handled outcome, not an error).
    auto psram_status = pico_toolset::psram_init(pico_toolset::configs::psram::kWaveshareRp2350PiZero);
    printf("PSRAM present=%d test_ok=%d size=%zu bytes\n",
           psram_status.present, psram_status.test_ok, psram_status.size_bytes);

    // -- SD card --
    SdCard sd;
    if (!sd.init(pico_toolset::configs::sdcard::kWaveshareRp2350PiZero)) {
        printf("SD mount failed (FRESULT=%d)\n", sd.last_mount_result());
    } else {
        printf("SD mounted (native SPI: %s)\n", sd.used_native_spi() ? "yes" : "no");
        for (const auto& name : sd.list_files({"txt"})) printf("  %s\n", name.c_str());
    }

    // -- USB HID host (Hdmi profile: core1 is taken by DVI, PIO0 is taken by
    // DVI, so this polls on core0 over PIO2 -- see usb_hid_configs.h) --
    UsbHidHost usb;
    usb.init(pico_toolset::configs::usb_hid::kWaveshareRp2350PiZeroHdmi);

    // -- HDMI (core1) --
    fill_test_pattern();
    g_dvi.timing = &dvi_timing_640x480p_60hz;
    g_dvi.ser_cfg = pico_sock_cfg; // this board's onboard TMDS connector
    dvi_init(&g_dvi, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    for (auto& buf : g_scanlines) {
        void* p = &buf;
        queue_add_blocking_u32(&g_dvi.q_colour_valid, &p);
    }
    multicore_launch_core1(core1_main);

    while (true) {
        void* p;
        if (queue_try_remove_u32(&g_dvi.q_colour_free, &p)) {
            queue_add_blocking_u32(&g_dvi.q_colour_valid, &p);
        }

        usb.task(); // run_on_core1=false in the Hdmi profile: poll it ourselves

        if (usb.connected_gamepad_count() > 0) {
            auto g = usb.gamepad_state(0);
            if (g.present) printf("pad0: btns=%04X\n", g.buttons);
        }
    }
}
