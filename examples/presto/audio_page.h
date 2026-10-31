// Bluetooth audio page of the Presto example (touch): pair/connect a headset, play/pause, volume,
// quality, sound. Included inside main.cpp's anonymous namespace, after kSize, kStatusH, kMenu* and
// draw_text_fit(), which it shares with the rest of the example; only for PRESTO_BT_A2DP builds.
#pragma once

// ---- Bluetooth audio page (touch): pair/connect a headset, play/pause, volume, quality, sound ----
enum class AudioAction { None, Pair, PlayPause, VolDown, VolUp, Quality, Sound, Forget, Back };

ToneSource g_sine(Waveform::Sine, 440.0f, 0.5f);
ToneSource g_square(Waveform::Square, 330.0f, 0.3f);
const AudioNote kScale[] = {{523, 200}, {587, 200}, {659, 200}, {698, 200}, {784, 200}, {880, 200}, {988, 200},
                            {1047, 400}, {0, 300}};
MelodySource g_melody(kScale, sizeof(kScale) / sizeof(kScale[0]));
SweepSource g_sweep(200.0f, 4000.0f, 5.0f, 0.4f);
AudioSource* const kSources[] = {&g_sine, &g_square, &g_melody, &g_sweep};
const char* const kSourceNames[] = {"SINE", "SQUARE", "MELODY", "SWEEP"};
constexpr size_t kSourceCount = sizeof(kSources) / sizeof(kSources[0]);

constexpr int kAudioButtonsY = kMenuY0 + 4 * kMenuRowH;
const char* const kAudioLabels[8] = {"PAIR", "PLAY/PAUSE", "VOL -", "VOL +", "QUALITY", "SOUND", "FORGET", "BACK"};
constexpr AudioAction kAudioActions[8] = {AudioAction::Pair,    AudioAction::PlayPause, AudioAction::VolDown,
                                          AudioAction::VolUp,   AudioAction::Quality,   AudioAction::Sound,
                                          AudioAction::Forget,  AudioAction::Back};

AudioAction audio_hit(int x, int y) {
    if (y < kAudioButtonsY) return AudioAction::None;
    const int row = (y - kAudioButtonsY) / kMenuRowH;
    if (row > 3) return AudioAction::None;
    return kAudioActions[row * 2 + (x < kSize / 2 ? 0 : 1)];
}

char g_a2dp_message[40] = "";
volatile uint32_t g_a2dp_event_ms = 0; // time of the last event, to show its message only briefly

void on_a2dp_event(const A2dpEvent& e, void*) {
    g_a2dp_event_ms = to_ms_since_boot(get_absolute_time());
    const auto& d = e.device;
#ifdef WANT_HCI_DUMP
    if (e.type == A2dpEventType::Connecting) hci_dump_enable_packet_log(true);
    else if (e.type == A2dpEventType::Disconnected || e.type == A2dpEventType::StreamStarted) hci_dump_enable_packet_log(false);
#endif
    const unsigned long now = to_ms_since_boot(get_absolute_time());
    switch (e.type) {
    case A2dpEventType::PairingStarted:
        std::snprintf(g_a2dp_message, sizeof(g_a2dp_message), "SET HEADSET IN PAIRING MODE");
        std::printf("[%lu ms] [a2dp] pairing: scanning for audio devices...\n", now);
        break;
    case A2dpEventType::PairingStopped: std::printf("[%lu ms] [a2dp] pairing stopped\n", now); break;
    case A2dpEventType::DeviceFound:
        std::printf("[%lu ms] [a2dp] found %02X:%02X:%02X:%02X:%02X:%02X '%s' cod 0x%06lX rssi %d\n", now, d.addr[0],
                    d.addr[1], d.addr[2], d.addr[3], d.addr[4], d.addr[5], d.name,
                    static_cast<unsigned long>(d.class_of_device), d.rssi);
        break;
    case A2dpEventType::Connecting:
        std::snprintf(g_a2dp_message, sizeof(g_a2dp_message), "CONNECTING %.20s", d.name);
        std::printf("[%lu ms] [a2dp] connecting...\n", now);
        break;
    case A2dpEventType::Capabilities:
        std::printf("[%lu ms] [a2dp] headset SBC capabilities: rates 0x%02X (48k=1 44.1k=2 32k=4 16k=8) modes 0x%02X "
                    "(joint=1 stereo=2 dual=4 mono=8) blocks 0x%02X subbands 0x%02X alloc 0x%02X bitpool %u..%u\n",
                    now, e.cap_rates, e.cap_modes, e.cap_blocks, e.cap_subbands, e.cap_alloc, e.cap_min_bitpool,
                    e.cap_max_bitpool);
        break;
    case A2dpEventType::Connected:
        std::snprintf(g_a2dp_message, sizeof(g_a2dp_message), "CONNECTED %.20s", d.name);
        std::printf("[%lu ms] [a2dp] connected '%s'\n", now, d.name);
        break;
    case A2dpEventType::Configured:
        std::printf("[%lu ms] [a2dp] SBC configured: %lu Hz, mode %d, blocks %u, subbands %u, bitpool %u (headset %u..%u) "
                    "= %lu kbps\n",
                    now, static_cast<unsigned long>(e.sbc.sample_rate), static_cast<int>(e.sbc.mode), e.blocks,
                    e.subbands, e.sbc.bitpool, e.min_bitpool, e.max_bitpool,
                    static_cast<unsigned long>(e.bitrate_bps / 1000));
        break;
    case A2dpEventType::StreamStarted: std::printf("[%lu ms] [a2dp] streaming\n", now); break;
    case A2dpEventType::StreamSuspended: std::printf("[%lu ms] [a2dp] stream suspended\n", now); break;
    case A2dpEventType::VolumeChanged: std::printf("[%lu ms] [a2dp] volume %u/127\n", now, e.volume); break;
    case A2dpEventType::RemoteKey:
        std::printf("[%lu ms] [a2dp] headset button %d (0 play, 1 pause, 2 stop, 3 next, 4 previous, 5 vol+, 6 vol-)\n",
                    now, static_cast<int>(e.remote_key));
        break;
    case A2dpEventType::Disconnected:
        std::snprintf(g_a2dp_message, sizeof(g_a2dp_message), "DISCONNECTED");
        std::printf("[%lu ms] [a2dp] disconnected (HCI reason 0x%02X, closed by %s)\n", now, e.status,
                    e.cause == A2dpEvent::CauseStack ? "the stack or the headset"
                    : e.cause == A2dpEvent::CauseDisconnectCall ? "disconnect() / console x"
                    : e.cause == A2dpEvent::CauseForget ? "forget_last() / console F" : "a quality change");
        break;
    case A2dpEventType::Failed:
        std::snprintf(g_a2dp_message, sizeof(g_a2dp_message), "FAILED 0x%02X", e.status);
        std::printf("[%lu ms] [a2dp] FAILED status 0x%02X\n", now, e.status);
        break;
    }
}

void draw_audio_page(DisplayDriver& d, const BluetoothA2dp& a2dp, size_t source_index) {
    d.fill_rect(0, kStatusH + 2, kSize - 1, kSize - 1, kColorBlack);
    static const char* const kStates[] = {"IDLE", "PAIRING", "CONNECTING", "CONNECTED", "STREAMING"};
    char line[48];
    draw_text_fit(d, 4, kMenuY0, "BT AUDIO", kColorWhite, kMenuScale);
    const A2dpState st = a2dp.state();
    std::snprintf(line, sizeof(line), "%s %.12s", kStates[static_cast<int>(st)],
                  a2dp.has_last_device() ? a2dp.last_device().name : "NO HEADSET");
    draw_text_fit(d, 4, kMenuY0 + kMenuRowH, line, st == A2dpState::Streaming ? kColorGreen : Color(0xFFE0), kMenuScale);
    std::snprintf(line, sizeof(line), "VOL %u/127 %lukbps", a2dp.volume(),
                  static_cast<unsigned long>(a2dp.active_bitrate_bps() / 1000));
    draw_text_fit(d, 4, kMenuY0 + 2 * kMenuRowH, line, kColorWhite, kMenuScale);
    std::snprintf(line, sizeof(line), "%s %s", a2dp_quality_name(a2dp.quality()), kSourceNames[source_index]);
    draw_text_fit(d, 4, kMenuY0 + 3 * kMenuRowH, line, Color(0x07FF), kMenuScale);
    for (int i = 0; i < 8; ++i) {
        const int x0 = (i % 2) * (kSize / 2) + 2, x1 = x0 + kSize / 2 - 5;
        const int y0 = kAudioButtonsY + (i / 2) * kMenuRowH;
        const Color bg = kAudioActions[i] == AudioAction::Forget ? Color(0xB000)
                       : kAudioActions[i] == AudioAction::Back   ? Color(0x4208) : Color(0x0320);
        d.fill_rect(x0, y0, x1, y0 + kMenuRowH - 2, bg);
        draw_text_fit(d, x0 + 4, y0 + 5, kAudioLabels[i], kColorWhite, kMenuScale, bg);
    }
}
