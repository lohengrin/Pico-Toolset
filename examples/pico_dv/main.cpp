// "pico-dv" carrier board full-combination example (doubles as an
// integration test): HDMI video-out + uSD card + I2S audio + 3 debounced
// buttons, all brought up together. See ../../boards/pico_dv_pico1.md (and
// its pico1w/pico2 siblings) for the board writeup this exercises.
//
// Like components/dvi_hdmi/example/dvi_hdmi_example.cpp, the DVI half of
// this has NOT been run on real "pico-dv" hardware from this repo -- the
// TMDS pin config (pimoroni_demo_hdmi_cfg, common_dvi_pin_configs.h) is a
// best-guess match by name/pin-non-conflict with the validated
// kPicoDvCarrier sdcard/i2s_audio presets, not independently re-validated
// here. Adapt DVI_DEFAULT_SERIAL_CONFIG below if your carrier differs -- see
// boards/pico_dv_pico1.md's "Notes/gotchas".
#include "dvi.h"
#include "common_dvi_pin_configs.h"
#include "pico_toolset/sdcard.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico_toolset/i2s_audio.h"
#include "pico_toolset/i2s_audio_configs.h"
#include "pico_toolset/reset_buttons.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "hardware/pio.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>

using pico_toolset::DebouncedButtons;
using pico_toolset::I2sAudioOutput;
using pico_toolset::SdCard;

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

// DVI needs a dedicated core for its scanbuf worker -- everything else
// (SD, I2S, buttons) runs from core0's main loop below.
void __not_in_flash_func(core1_main)() {
    dvi_register_irqs_this_core(&g_dvi, DMA_IRQ_0);
    dvi_start(&g_dvi);
    dvi_scanbuf_main_16bpp(&g_dvi);
}

} // namespace

int main() {
    stdio_init_all();
    sleep_ms(2000);
    printf("pico-dv example: HDMI + SD + I2S + buttons\n");

    uint32_t pending_tag = 0;
    if (pico_toolset::consume_pending_watchdog_tag(pending_tag)) {
        printf("Rebooted with tag %u\n", pending_tag);
    }

    // -- SD card --
    SdCard sd;
    if (!sd.init(pico_toolset::configs::sdcard::kPicoDvCarrier)) {
        printf("SD mount failed (FRESULT=%d)\n", sd.last_mount_result());
    } else {
        printf("SD mounted (native SPI: %s)\n", sd.used_native_spi() ? "yes" : "no");
        for (const auto& name : sd.list_files({"txt"})) printf("  %s\n", name.c_str());
    }

    // -- I2S audio (continuous 440Hz tone) --
    I2sAudioOutput audio;
    bool audio_ok = audio.init(pico_toolset::configs::i2s_audio::kPicoDvCarrier);
    printf("I2S audio init %s\n", audio_ok ? "ok" : "FAILED");
    constexpr float kToneHz = 440.0f;
    constexpr int kFrameSamples = 882; // matches kPicoDvCarrier.samples_per_buffer
    std::array<float, kFrameSamples> frame{};
    double phase = 0.0;
    const double phase_step = 2.0 * 3.14159265358979 * kToneHz / 44100.0;

    // -- 3 debounced buttons --
    // Not yet validated on real "pico-dv" hardware in this repo (no preset
    // exists in reset_buttons.h) -- adapt these pins for your board.
    constexpr std::array<uint8_t, 3> kButtonPins = {14, 15, 16};
    DebouncedButtons buttons;
    buttons.init(kButtonPins);

    // -- HDMI (core1) --
    fill_test_pattern();
    g_dvi.timing = &dvi_timing_640x480p_60hz;
    g_dvi.ser_cfg = pimoroni_demo_hdmi_cfg; // see file header comment
    dvi_init(&g_dvi, next_striped_spin_lock_num(), next_striped_spin_lock_num());
    for (auto& buf : g_scanlines) {
        void* p = &buf;
        queue_add_blocking_u32(&g_dvi.q_colour_valid, &p);
    }
    multicore_launch_core1(core1_main);

    while (true) {
        // Recycle DVI scanline buffers (never redrawn -- see dvi_hdmi_example.cpp).
        void* p;
        if (queue_try_remove_u32(&g_dvi.q_colour_free, &p)) {
            queue_add_blocking_u32(&g_dvi.q_colour_valid, &p);
        }

        if (audio_ok) {
            for (float& s : frame) {
                s = 0.2f * static_cast<float>(std::sin(phase));
                phase += phase_step;
            }
            audio.queue_samples(frame);
        }

        int pressed = buttons.poll();
        if (pressed >= 0) {
            printf("Button %d debounced -- rebooting with tag %d\n", pressed, pressed);
            pico_toolset::watchdog_reboot_with_tag(static_cast<uint32_t>(pressed));
        }
    }
}
