// A2DP source smoke test: streams a generated tone or melody to a Bluetooth headset.
// Put the headset in pairing mode, then use the serial console:
//   a  pair a new headset (inquiry for audio devices, connects to the first one found)
//   c  connect to the last headset     x  disconnect     F  forget the last headset
//   p  pause / play                    + / -  volume
//   q  next quality preset (High -> Medium -> Low -> Mono -> MonoLow)
//   w  next sound: sine, square, melody, sweep      s  print stream statistics
// Needs a CYW43 board (PICO_BOARD=pimoroni_presto or pico2_w) and a build with
// PICO_TOOLSET_BLUETOOTH_CLASSIC=ON.
#include <cstdio>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico_toolset/audio_synth.h"
#include "pico_toolset/bluetooth.h"
#include "pico_toolset/bluetooth_a2dp.h"

using namespace pico_toolset;

namespace {

ToneSource g_sine(Waveform::Sine, 440.0f, 0.5f);
ToneSource g_square(Waveform::Square, 330.0f, 0.3f);
const AudioNote kScale[] = {{523, 200}, {587, 200}, {659, 200}, {698, 200}, {784, 200}, {880, 200}, {988, 200},
                            {1047, 400}, {0, 300}};
MelodySource g_melody(kScale, sizeof(kScale) / sizeof(kScale[0]));
SweepSource g_sweep(200.0f, 4000.0f, 5.0f, 0.4f);
AudioSource* const kSources[] = {&g_sine, &g_square, &g_melody, &g_sweep};
const char* const kSourceNames[] = {"sine 440 Hz", "square 330 Hz", "melody", "sweep"};

void on_event(const A2dpEvent& e, void*) {
    const auto& d = e.device;
    switch (e.type) {
    case A2dpEventType::PairingStarted: std::printf("[a2dp] pairing: scanning for audio devices...\n"); break;
    case A2dpEventType::PairingStopped: std::printf("[a2dp] pairing stopped\n"); break;
    case A2dpEventType::DeviceFound:
        std::printf("[a2dp] found %02X:%02X:%02X:%02X:%02X:%02X '%s' cod 0x%06lX rssi %d\n", d.addr[0], d.addr[1],
                    d.addr[2], d.addr[3], d.addr[4], d.addr[5], d.name, static_cast<unsigned long>(d.class_of_device),
                    d.rssi);
        break;
    case A2dpEventType::Connecting: std::printf("[a2dp] connecting...\n"); break;
    case A2dpEventType::Capabilities:
        std::printf("[a2dp] headset SBC capabilities: rates 0x%02X (48k=1 44.1k=2 32k=4 16k=8) modes 0x%02X "
                    "(joint=1 stereo=2 dual=4 mono=8) blocks 0x%02X subbands 0x%02X alloc 0x%02X bitpool %u..%u\n",
                    e.cap_rates, e.cap_modes, e.cap_blocks, e.cap_subbands, e.cap_alloc, e.cap_min_bitpool,
                    e.cap_max_bitpool);
        break;
    case A2dpEventType::Connected: std::printf("[a2dp] connected '%s'\n", d.name); break;
    case A2dpEventType::Configured:
        std::printf("[a2dp] SBC configured: %lu Hz, mode %d, blocks %u, subbands %u, bitpool %u (headset %u..%u) = %lu kbps\n",
                    static_cast<unsigned long>(e.sbc.sample_rate), static_cast<int>(e.sbc.mode), e.blocks, e.subbands,
                    e.sbc.bitpool, e.min_bitpool, e.max_bitpool, static_cast<unsigned long>(e.bitrate_bps / 1000));
        break;
    case A2dpEventType::StreamStarted: std::printf("[a2dp] streaming\n"); break;
    case A2dpEventType::StreamSuspended: std::printf("[a2dp] stream suspended\n"); break;
    case A2dpEventType::VolumeChanged: std::printf("[a2dp] volume %u/127\n", e.volume); break;
    case A2dpEventType::Disconnected: std::printf("[a2dp] disconnected\n"); break;
    case A2dpEventType::Failed: std::printf("[a2dp] FAILED status 0x%02X\n", e.status); break;
    }
}

} // namespace

int main() {
    stdio_init_all();
    sleep_ms(2000);
    if (cyw43_arch_init()) return 1;

    Bluetooth bt;
    if (!bt.init({})) { std::printf("bluetooth init failed\n"); return 1; }

    BluetoothA2dp a2dp;
    A2dpConfig cfg;
    cfg.source = &g_sine;
    cfg.callback = on_event;
    if (!a2dp.init(cfg)) { std::printf("a2dp init failed\n"); return 1; }
    std::printf("A2DP source ready (quality %s). Last headset: %s\n", a2dp_quality_name(a2dp.quality()),
                a2dp.has_last_device() ? a2dp.last_device().name : "none");

    size_t source_index = 0;
    uint8_t volume = a2dp.volume();
    while (true) {
        const int c = getchar_timeout_us(10000);
        switch (c) {
        case 'a': a2dp.start_pairing(); break;
        case 'c': a2dp.connect_last(); break;
        case 'x': a2dp.disconnect(); break;
        case 'F': a2dp.forget_last(); break;
        case 'p': a2dp.state() == A2dpState::Streaming ? a2dp.pause() : a2dp.play(); break;
        case '+': volume = volume > 117 ? 127 : volume + 10; a2dp.set_volume(volume); break;
        case '-': volume = volume < 10 ? 0 : volume - 10; a2dp.set_volume(volume); break;
        case 'q': {
            const A2dpQuality next = static_cast<A2dpQuality>((static_cast<int>(a2dp.quality()) + 1) %
                                                              (static_cast<int>(A2dpQuality::MonoLow) + 1));
            a2dp.set_quality(next);
            std::printf("quality -> %s (~%lu kbps before negotiation)\n", a2dp_quality_name(next),
                        static_cast<unsigned long>(a2dp_sbc_bitrate_bps(a2dp_quality_config(next).sample_rate,
                                                                       a2dp_quality_config(next).mode,
                                                                       a2dp_quality_config(next).bitpool) / 1000));
            break;
        }
        case 'w':
            source_index = (source_index + 1) % (sizeof(kSources) / sizeof(kSources[0]));
            a2dp.set_source(kSources[source_index]);
            std::printf("sound -> %s\n", kSourceNames[source_index]);
            break;
        case 's': {
            const A2dpStats s = a2dp.stats();
            std::printf("packets %lu, sbc frames %lu, bytes %lu, dropped %lu, %lu kbps, remote volume %d\n",
                        static_cast<unsigned long>(s.packets_sent), static_cast<unsigned long>(s.sbc_frames_sent),
                        static_cast<unsigned long>(s.bytes_sent), static_cast<unsigned long>(s.dropped_frames),
                        static_cast<unsigned long>(a2dp.active_bitrate_bps() / 1000), a2dp.remote_volume_active());
            break;
        }
        default: break;
        }
    }
}
