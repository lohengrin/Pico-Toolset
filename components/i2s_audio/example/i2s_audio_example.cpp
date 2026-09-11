// I2S audio output example (doubles as an integration test).
// Plays a continuous sine wave through a PCM5100A-style I2S DAC.
//
// Requires pico-extras' pico_audio_i2s target to already exist in the
// consuming build -- see this component's CMakeLists.txt and the toolset
// README's "I2S audio (pico-extras)" section for the required
// pico_extras_import.cmake setup (must run before project(), same
// constraint as pico_sdk_import.cmake, so it cannot happen here).
#include "pico_toolset/i2s_audio.h"
#include "pico/stdlib.h"

#include <array>
#include <cmath>
#include <cstdio>

using pico_toolset::I2sAudioConfig;
using pico_toolset::I2sAudioOutput;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    I2sAudioConfig cfg;
    // Adapt pins/DMA channel/PIO SM here for your board -- pick a DMA
    // channel none of your other drivers also claim (see the config
    // struct's own doc comment).
    cfg.sample_rate_hz = 44100;
    cfg.pin_data = 26;
    cfg.pin_clock_base = 27; // BCK=27, LRCK=28
    cfg.dma_channel = 0;
    cfg.pio_sm = 0;

    I2sAudioOutput audio;
    if (!audio.init(cfg)) {
        printf("I2S audio init failed\n");
        return 1;
    }
    printf("I2S audio init ok\n");

    constexpr float kToneHz = 440.0f;
    constexpr int kFrameSamples = 882; // matches cfg.samples_per_buffer default
    std::array<float, kFrameSamples> frame{};
    double phase = 0.0;
    const double phase_step = 2.0 * 3.14159265358979 * kToneHz / cfg.sample_rate_hz;

    while (true) {
        for (float& s : frame) {
            s = 0.2f * static_cast<float>(std::sin(phase));
            phase += phase_step;
        }
        audio.queue_samples(frame);
        sleep_ms(1); // pace roughly to buffer duration; real callers drive this from their own loop
    }
}
