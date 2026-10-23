#pragma once

#include <cstdint>

#include "pico_toolset/audio_source.h"

namespace pico_toolset {

// ---- SBC quality ----------------------------------------------------------------------
//
// A2DP audio is SBC. The bit rate follows from sample rate, channel mode and bitpool
// (blocks/subbands/allocation are negotiated with the headset, 16/8/loudness typically).
// Note that sinks must support 44.1 and 48 kHz; 16 and 32 kHz are optional for them, so the
// presets keep 44.1 kHz and save bandwidth with mono / a lower bitpool instead.

enum class A2dpChannelMode : uint8_t { Mono, Dual, Stereo, JointStereo };

struct A2dpSbcConfig {
    uint32_t        sample_rate;  // 16000, 32000, 44100 or 48000
    A2dpChannelMode mode;
    uint8_t         bitpool;      // 2..53 (clamped into the headset's accepted range)
};

enum class A2dpQuality : uint8_t {
    High,     // 44.1 kHz joint stereo, bitpool 53, ~328 kbps (the A2DP "high quality" point)
    Medium,   // 44.1 kHz joint stereo, bitpool 35, ~229 kbps
    Low,      // 44.1 kHz joint stereo, bitpool 25, ~174 kbps (default)
    Mono,     // 44.1 kHz mono, bitpool 24, ~154 kbps
    MonoLow,  // 44.1 kHz mono, bitpool 16, ~110 kbps
    Custom,   // A2dpConfig::custom (e.g. 32 or 16 kHz mono if the headset offers them)
};

[[nodiscard]] A2dpSbcConfig a2dp_quality_config(A2dpQuality quality);
[[nodiscard]] const char* a2dp_quality_name(A2dpQuality quality);
// SBC bit rate in bit/s for the given parameters.
[[nodiscard]] uint32_t a2dp_sbc_bitrate_bps(uint32_t sample_rate, A2dpChannelMode mode, uint8_t bitpool,
                                            uint8_t blocks = 16, uint8_t subbands = 8);

// ---- Events ----------------------------------------------------------------------------

enum class A2dpEventType : uint8_t {
    PairingStarted,   // inquiry for audio devices begins
    PairingStopped,
    DeviceFound,      // an audio device answered (device valid); connect() unless auto_connect
    Connecting,       // establishing the stream to `device`
    Capabilities,     // the headset's SBC capabilities (bitmaps below), logged for diagnostics
    Connected,        // signalling channel up and device remembered
    Configured,       // negotiated SBC parameters (sbc/blocks/subbands/bitpool range/bitrate)
    StreamStarted,
    StreamSuspended,
    VolumeChanged,    // `volume` 0..127, from the headset's buttons or from set_volume()
    Disconnected,
    Failed,           // `status` = BTstack/HCI error
};

struct A2dpDevice {
    uint8_t  addr[6];
    char     name[24];
    int8_t   rssi;
    uint32_t class_of_device;
};

struct A2dpEvent {
    A2dpEventType type;
    A2dpDevice    device;
    // Configured:
    A2dpSbcConfig sbc;
    uint8_t       blocks, subbands, min_bitpool, max_bitpool;
    uint32_t      bitrate_bps;
    // Capabilities (AVDTP bit layouts: rate 48k=1,44.1k=2,32k=4,16k=8; mode joint=1,stereo=2,dual=4,mono=8):
    uint8_t       cap_rates, cap_modes, cap_blocks, cap_subbands, cap_alloc, cap_min_bitpool, cap_max_bitpool;
    uint8_t       volume;
    uint8_t       status;
    // Disconnected: why this side closed the link (0 = not us / the stack decided).
    enum Cause : uint8_t { CauseStack, CauseDisconnectCall, CauseForget, CauseQualityChange } cause;
};

// Called from the BTstack context: keep it short and only record state.
using A2dpEventCallback = void (*)(const A2dpEvent& event, void* user);

enum class A2dpVolumeMode : uint8_t {
    Auto,      // the headset's own volume (AVRCP absolute volume) if it supports it, else software gain
    Software,  // always scale the PCM before encoding
    Remote,    // only send AVRCP volume (no effect on headsets that do not support it)
};

enum class A2dpState : uint8_t { Idle, Pairing, Connecting, Connected, Streaming };

// Behavior only -- the radio wiring comes from the board header.
struct A2dpConfig {
    A2dpQuality    quality = A2dpQuality::Low;
    A2dpSbcConfig  custom{44100, A2dpChannelMode::JointStereo, 25}; // used when quality == Custom
    AudioSource*   source = nullptr;      // null = silence
    A2dpEventCallback callback = nullptr;
    void*          user = nullptr;
    const char*    local_name = "pico-toolset";  // name the headset sees
    bool           auto_play = true;      // start streaming as soon as the stream is open
    bool           auto_connect = true;   // pairing mode connects to the first audio device found
    bool           auto_reconnect = true; // one attempt to the last headset after init()
    uint32_t       pairing_timeout_ms = 30000;
    A2dpVolumeMode volume_mode = A2dpVolumeMode::Auto;
    uint8_t        initial_volume = 64;   // 0..127
    // Audio that could not be sent yet is kept up to this long (it is generated on demand, so this
    // costs no memory); beyond it the oldest is dropped. A packet holds ~32 ms of audio and the
    // radio stalls now and then, so a small value turns every stall into an audible cut.
    uint16_t       max_backlog_ms = 250;  // 40..1000
    const char*    pin = "0000";          // legacy PIN answer for old headsets
};

struct A2dpStats {
    uint32_t packets_sent;
    uint32_t sbc_frames_sent;
    uint32_t bytes_sent;
    uint32_t dropped_frames;  // audio discarded because the radio could not keep up
    uint32_t encode_us_total; // time spent producing + encoding SBC frames
    uint32_t encode_count;    // number of frames encoded (average = total / count)
    uint32_t encode_us_max;
};

// Bluetooth A2DP **source**: streams generated audio (an AudioSource) to a headset or speaker.
// Playback only (no microphone / hands-free). Requires Bluetooth::init() and a build with
// PICO_TOOLSET_BLUETOOTH_CLASSIC. Optional: not compiled unless this component is linked.
class BluetoothA2dp {
public:
    bool init(const A2dpConfig& config);

    // --- Finding a headset ---
    void start_pairing();   // put the headset in pairing mode first
    void stop_pairing();
    bool connect(const uint8_t addr[6]);
    bool connect_last();    // the headset used last time (stored in flash)
    void disconnect();
    void forget_last();

    // --- Playback ---
    void play();            // start (or resume) the stream
    void pause();           // suspend the stream, keep the connection
    void set_source(AudioSource* source);

    // The preset applies to the next stream; if the connection is up and the sample rate and
    // channel mode of the new preset differ, the connection is re-established. A pure bitpool
    // change takes effect immediately.
    void set_quality(A2dpQuality quality);
    void set_custom(const A2dpSbcConfig& config); // also selects A2dpQuality::Custom
    [[nodiscard]] A2dpQuality quality() const;

    // 0..127 (AVRCP absolute volume range).
    void set_volume(uint8_t volume);
    [[nodiscard]] uint8_t volume() const;
    [[nodiscard]] bool remote_volume_active() const;

    // --- Status ---
    [[nodiscard]] A2dpState state() const;
    [[nodiscard]] bool has_last_device() const;
    [[nodiscard]] A2dpDevice last_device() const;
    [[nodiscard]] A2dpSbcConfig active_config() const; // negotiated, valid once Configured
    [[nodiscard]] uint32_t active_bitrate_bps() const;
    [[nodiscard]] A2dpStats stats() const;
};

} // namespace pico_toolset
