// A2DP source (playback only) on BTstack, following BTstack's a2dp_source_demo: SDP records for
// A2DP source + AVRCP controller, one SBC stream endpoint, a paced timer that pulls PCM from an
// AudioSource, encodes it with the Bluedroid SBC encoder and sends RTP packets when AVDTP says it
// can. Quality presets choose sample rate / channel mode (preferred values for the negotiation)
// and the bitpool; AVRCP absolute volume (or a software gain) controls the volume.
#ifdef ENABLE_CLASSIC

#include "pico_toolset/bluetooth_a2dp.h"

#include <cstdio>
#include <cstring>

#include "btstack.h"
#include "btstack_tlv.h"
#include "pico/async_context.h"
#include "pico/cyw43_arch.h"
#include "pico_toolset/bluetooth_classic.h"

namespace pico_toolset {

namespace {

constexpr uint32_t kTlvTag = (static_cast<uint32_t>('A') << 24) | (static_cast<uint32_t>('2') << 16) |
                             (static_cast<uint32_t>('D') << 8) | 'P';
constexpr uint32_t kAudioTimerMs = 10;
// Largest SBC payload we assemble; the real limit is the L2CAP MTU the headset grants.
#ifndef PICO_TOOLSET_A2DP_SBC_STORAGE_SIZE
#define PICO_TOOLSET_A2DP_SBC_STORAGE_SIZE 700
#endif
constexpr size_t kSbcStorage = PICO_TOOLSET_A2DP_SBC_STORAGE_SIZE;
constexpr uint32_t kMaxBacklogMs = 60; // beyond this much unsent audio, drop instead of bursting

struct Stored {
    uint8_t addr[6];
    char name[24];
};

// SDP records and endpoint capabilities (rates/modes advertised: everything the encoder can do;
// the headset's capabilities and our preferences decide what is actually configured).
uint8_t g_sdp_a2dp_source[150];
uint8_t g_sdp_avrcp_controller[200];
uint8_t g_sbc_capabilities[4] = {
    static_cast<uint8_t>(((AVDTP_SBC_16000 | AVDTP_SBC_32000 | AVDTP_SBC_44100 | AVDTP_SBC_48000) << 4) |
                         AVDTP_SBC_MONO | AVDTP_SBC_DUAL_CHANNEL | AVDTP_SBC_STEREO | AVDTP_SBC_JOINT_STEREO),
    0xFF, // all block lengths, both subband counts, both allocation methods
    2, 53,
};
uint8_t g_sbc_configuration[4] = {
    static_cast<uint8_t>((AVDTP_SBC_44100 << 4) | AVDTP_SBC_STEREO),
    static_cast<uint8_t>((AVDTP_SBC_BLOCK_LENGTH_16 << 4) | (AVDTP_SBC_SUBBANDS_8 << 2) |
                         AVDTP_SBC_ALLOCATION_METHOD_LOUDNESS),
    2, 53,
};

struct State {
    bool inited = false;
    A2dpConfig cfg{};
    AudioSource* source = nullptr;
    A2dpQuality quality = A2dpQuality::Low;
    A2dpSbcConfig preferred{44100, A2dpChannelMode::JointStereo, 25};

    avdtp_stream_endpoint_t* endpoint = nullptr;
    uint8_t local_seid = 0, remote_seid = 0;
    uint16_t a2dp_cid = 0, avrcp_cid = 0;
    bd_addr_t peer{};
    char peer_name[24]{};
    bool have_last = false;
    Stored last{};

    A2dpState state = A2dpState::Idle;
    bool initiated = false;  // we started the connection (else the headset called us)
    bool stream_open = false;
    bool streaming = false;
    bool restart_pending = false; // reconnect after a disconnect (quality change)

    btstack_sbc_encoder_bluedroid_t enc_state{};
    const btstack_sbc_encoder_t* enc = nullptr;
    struct {
        bool valid = false;
        uint32_t rate = 44100;
        uint8_t channels = 2;
        A2dpChannelMode mode = A2dpChannelMode::JointStereo;
        uint8_t blocks = 16, subbands = 8, min_bitpool = 2, max_bitpool = 53, bitpool = 25;
        btstack_sbc_allocation_method_t alloc = static_cast<btstack_sbc_allocation_method_t>(0); // loudness
        uint32_t bitrate = 0;
    } active;

    uint8_t sbc_storage[kSbcStorage + 1]{};
    uint16_t sbc_count = 0;
    bool ready_to_send = false;
    uint16_t max_payload = 0;
    uint32_t rtp_timestamp = 0;
    uint32_t samples_ready = 0;
    uint32_t acc_missed = 0;
    uint32_t t_last = 0;
    int16_t pcm[128 * 2]{}; // one SBC frame: blocks * subbands (<= 128) frames, <= 2 channels
    btstack_timer_source_t audio_timer{};

    uint8_t volume = 64;
    bool remote_abs = false;     // headset accepts AVRCP absolute volume
    int32_t gain_q15 = 32767;    // software gain

    bool pairing = false;
    bool inquiry_seen = false;
    btstack_timer_source_t pair_timer{};
    btstack_timer_source_t reconnect_timer{};
    btstack_packet_callback_registration_t hci_reg{};
    btstack_timer_source_t bond_timer{};
    bool bonding = false; // dedicated bonding in progress before the A2DP connection

    A2dpStats stats{};
};
State g;

struct ContextLock {
    ContextLock() { async_context_acquire_lock_blocking(cyw43_arch_async_context()); }
    ~ContextLock() { async_context_release_lock(cyw43_arch_async_context()); }
};

uint8_t mode_bit(A2dpChannelMode m) {
    switch (m) {
    case A2dpChannelMode::Mono: return AVDTP_SBC_MONO;
    case A2dpChannelMode::Dual: return AVDTP_SBC_DUAL_CHANNEL;
    case A2dpChannelMode::Stereo: return AVDTP_SBC_STEREO;
    default: return AVDTP_SBC_JOINT_STEREO;
    }
}

A2dpDevice device_info() {
    A2dpDevice d{};
    std::memcpy(d.addr, g.peer, 6);
    std::memcpy(d.name, g.peer_name, sizeof(d.name));
    return d;
}

void emit(A2dpEventType type, uint8_t status = 0) {
    if (!g.cfg.callback) return;
    A2dpEvent e{};
    e.type = type;
    e.device = device_info();
    e.sbc = {g.active.rate, g.active.mode, g.active.bitpool};
    e.blocks = g.active.blocks;
    e.subbands = g.active.subbands;
    e.min_bitpool = g.active.min_bitpool;
    e.max_bitpool = g.active.max_bitpool;
    e.bitrate_bps = g.active.bitrate;
    e.volume = g.volume;
    e.status = status;
    g.cfg.callback(e, g.cfg.user);
}

// ---- persistence ----
void save_last() {
    const btstack_tlv_t* tlv = nullptr;
    void* ctx = nullptr;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (!tlv) return;
    std::memcpy(g.last.addr, g.peer, 6);
    std::memcpy(g.last.name, g.peer_name, sizeof(g.last.name));
    g.have_last = true;
    tlv->store_tag(ctx, kTlvTag, reinterpret_cast<const uint8_t*>(&g.last), sizeof(g.last));
}
void load_last() {
    const btstack_tlv_t* tlv = nullptr;
    void* ctx = nullptr;
    btstack_tlv_get_instance(&tlv, &ctx);
    g.have_last = tlv && tlv->get_tag(ctx, kTlvTag, reinterpret_cast<uint8_t*>(&g.last), sizeof(g.last)) ==
                             static_cast<int>(sizeof(g.last));
}
void erase_last() {
    const btstack_tlv_t* tlv = nullptr;
    void* ctx = nullptr;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (tlv) tlv->delete_tag(ctx, kTlvTag);
    g.have_last = false;
}

// ---- volume ----
void apply_volume() {
    // Perceptual curve for the software gain; unity when the headset renders the volume itself.
    const bool remote = g.cfg.volume_mode != A2dpVolumeMode::Software && g.remote_abs && g.avrcp_cid;
    if (remote) {
        g.gain_q15 = 32767;
        avrcp_controller_set_absolute_volume(g.avrcp_cid, g.volume);
    } else if (g.cfg.volume_mode == A2dpVolumeMode::Remote) {
        g.gain_q15 = 32767; // remote only: nothing to scale
    } else {
        const int32_t v = g.volume;
        g.gain_q15 = (v * v * 32767) / (127 * 127);
    }
}

// ---- encoder / media ----
void produce_audio(int16_t* pcm, unsigned frames) {
    if (g.source) g.source->fill(pcm, frames);
    else std::memset(pcm, 0, frames * g.active.channels * sizeof(int16_t));
    if (g.gain_q15 != 32767) {
        const unsigned n = frames * g.active.channels;
        for (unsigned i = 0; i < n; ++i) pcm[i] = static_cast<int16_t>((pcm[i] * g.gain_q15) >> 15);
    }
}

void send_media_packet() {
    const int frame_bytes = g.enc->sbc_buffer_length(&g.enc_state);
    const uint8_t frames = static_cast<uint8_t>(g.sbc_count / frame_bytes);
    g.sbc_storage[0] = frames; // SBC media payload header: number of frames
    a2dp_source_stream_send_media_payload_rtp(g.a2dp_cid, g.local_seid, 0, g.rtp_timestamp, g.sbc_storage,
                                              g.sbc_count + 1);
    g.rtp_timestamp += static_cast<uint32_t>(frames) * g.enc->num_audio_frames(&g.enc_state);
    g.stats.packets_sent++;
    g.stats.sbc_frames_sent += frames;
    g.stats.bytes_sent += g.sbc_count + 1u;
    g.sbc_count = 0;
    g.ready_to_send = false;
}

void fill_sbc_storage() {
    const unsigned per_frame = g.enc->num_audio_frames(&g.enc_state);
    const uint16_t frame_bytes = g.enc->sbc_buffer_length(&g.enc_state);
    while (g.samples_ready >= per_frame && (g.max_payload - g.sbc_count) >= frame_bytes) {
        produce_audio(g.pcm, per_frame);
        g.enc->encode_signed_16(&g.enc_state, g.pcm, &g.sbc_storage[1 + g.sbc_count]);
        g.sbc_count += frame_bytes;
        g.samples_ready -= per_frame;
    }
}

void audio_timeout(btstack_timer_source_t*) {
    btstack_run_loop_set_timer(&g.audio_timer, kAudioTimerMs);
    btstack_run_loop_add_timer(&g.audio_timer);

    const uint32_t now = btstack_run_loop_get_time_ms();
    uint32_t elapsed = kAudioTimerMs;
    if (g.t_last) elapsed = now - g.t_last;
    g.t_last = now;
    uint32_t samples = (elapsed * g.active.rate) / 1000;
    g.acc_missed += (elapsed * g.active.rate) % 1000;
    while (g.acc_missed >= 1000) { ++samples; g.acc_missed -= 1000; }
    g.samples_ready += samples;

    // Radio too slow: drop the oldest audio rather than sending a burst later.
    const uint32_t max_backlog = g.active.rate * kMaxBacklogMs / 1000;
    if (g.samples_ready > max_backlog) {
        const unsigned per_frame = g.enc->num_audio_frames(&g.enc_state);
        const uint32_t drop = (g.samples_ready - max_backlog) / per_frame * per_frame;
        g.samples_ready -= drop;
        g.stats.dropped_frames += drop / per_frame;
    }

    if (g.ready_to_send) return;
    fill_sbc_storage();
    if (g.sbc_count + g.enc->sbc_buffer_length(&g.enc_state) > g.max_payload) {
        g.ready_to_send = true;
        a2dp_source_stream_endpoint_request_can_send_now(g.a2dp_cid, g.local_seid);
    }
}

void media_start() {
    if (g.streaming) return;
    if (g.source) g.source->prepare(g.active.rate, g.active.channels);
    g.max_payload = static_cast<uint16_t>(btstack_min(a2dp_max_media_payload_size(g.a2dp_cid, g.local_seid),
                                                      static_cast<int>(kSbcStorage)));
    g.sbc_count = 0;
    g.ready_to_send = false;
    g.samples_ready = 0;
    g.acc_missed = 0;
    g.t_last = 0;
    g.streaming = true;
    btstack_run_loop_remove_timer(&g.audio_timer);
    btstack_run_loop_set_timer_handler(&g.audio_timer, audio_timeout);
    btstack_run_loop_set_timer(&g.audio_timer, kAudioTimerMs);
    btstack_run_loop_add_timer(&g.audio_timer);
}

void media_stop() {
    g.streaming = false;
    g.sbc_count = 0;
    g.ready_to_send = false;
    g.samples_ready = 0;
    btstack_run_loop_remove_timer(&g.audio_timer);
}

// Encoder bitpool: the preset's, clamped into what the headset accepts.
uint8_t chosen_bitpool(uint8_t min_bp, uint8_t max_bp) {
    uint8_t bp = g.preferred.bitpool;
    if (bp > max_bp) bp = max_bp;
    if (bp < min_bp) bp = min_bp;
    return bp;
}

void reconfigure_encoder_bitpool() {
    if (!g.active.valid) return;
    const uint8_t bp = chosen_bitpool(g.active.min_bitpool, g.active.max_bitpool);
    if (bp == g.active.bitpool) return;
    g.active.bitpool = bp;
    g.enc->configure(&g.enc_state, SBC_MODE_STANDARD, g.active.blocks, g.active.subbands,
                     g.active.alloc, static_cast<uint16_t>(g.active.rate), bp,
                     g.active.mode == A2dpChannelMode::Mono ? SBC_CHANNEL_MODE_MONO
                     : g.active.mode == A2dpChannelMode::Dual ? SBC_CHANNEL_MODE_DUAL_CHANNEL
                     : g.active.mode == A2dpChannelMode::Stereo ? SBC_CHANNEL_MODE_STEREO
                                                                : SBC_CHANNEL_MODE_JOINT_STEREO);
    g.active.bitrate = static_cast<uint32_t>((static_cast<uint64_t>(g.enc->sbc_buffer_length(&g.enc_state)) * 8u *
                                              g.active.rate) / g.enc->num_audio_frames(&g.enc_state));
}

void set_preferences() {
    if (!g.endpoint) return;
    avdtp_set_preferred_sampling_frequency(g.endpoint, g.preferred.sample_rate);
    avdtp_set_preferred_channel_mode(g.endpoint, mode_bit(g.preferred.mode));
}

A2dpSbcConfig resolve_quality() {
    return g.quality == A2dpQuality::Custom ? g.cfg.custom : a2dp_quality_config(g.quality);
}

// ---- connection management ----
bool open_stream() {
    set_preferences();
    const uint8_t rc = a2dp_source_establish_stream(g.peer, &g.a2dp_cid);
    if (rc != ERROR_CODE_SUCCESS) {
        g.state = A2dpState::Idle;
        emit(A2dpEventType::Failed, rc);
        return false;
    }
    return true;
}

void bond_done_timeout(btstack_timer_source_t*) {
    if (g.state == A2dpState::Connecting) open_stream();
}

// Headsets (older Sony ones in particular) only answer the SDP query for their A2DP sink record
// on an authenticated link, so a new device is bonded first, like the Classic HID path does.
bool start_connection(const uint8_t addr[6]) {
    if (g.state == A2dpState::Connecting || g.state == A2dpState::Connected || g.state == A2dpState::Streaming) return false;
    std::memcpy(g.peer, addr, 6);
    g.initiated = true;
    g.state = A2dpState::Connecting;
    emit(A2dpEventType::Connecting);
    link_key_t key;
    link_key_type_t key_type;
    if (!gap_get_link_key_for_bd_addr(g.peer, key, &key_type)) {
        g.bonding = true;
        const uint8_t rc = gap_dedicated_bonding(g.peer, 0);
        if (rc != ERROR_CODE_SUCCESS) {
            g.bonding = false;
            g.state = A2dpState::Idle;
            emit(A2dpEventType::Failed, rc);
            return false;
        }
        return true;
    }
    return open_stream();
}

void hci_handler(uint8_t packet_type, uint16_t, uint8_t* packet, uint16_t) {
    if (packet_type != HCI_EVENT_PACKET || hci_event_packet_get_type(packet) != GAP_EVENT_DEDICATED_BONDING_COMPLETED ||
        !g.bonding)
        return;
    g.bonding = false;
    const uint8_t status = gap_event_dedicated_bonding_completed_get_status(packet);
    if (status != ERROR_CODE_SUCCESS) {
        g.state = A2dpState::Idle;
        emit(A2dpEventType::Failed, status);
        return;
    }
    // BTstack is still tearing down the bonding link inside this event; a connection started
    // right here fails, so open the stream a moment later.
    btstack_run_loop_remove_timer(&g.bond_timer);
    btstack_run_loop_set_timer_handler(&g.bond_timer, bond_done_timeout);
    btstack_run_loop_set_timer(&g.bond_timer, 500);
    btstack_run_loop_add_timer(&g.bond_timer);
}

void stop_pairing_internal() {
    if (!g.pairing) return;
    g.pairing = false;
    btstack_run_loop_remove_timer(&g.pair_timer);
    BluetoothClassic::stop_inquiry();
    if (g.state == A2dpState::Pairing) g.state = A2dpState::Idle;
    emit(A2dpEventType::PairingStopped);
}
void pair_timeout(btstack_timer_source_t*) { stop_pairing_internal(); }

void reconnect_timeout(btstack_timer_source_t*) {
    if (g.state == A2dpState::Idle && g.have_last) start_connection(g.last.addr);
}
void schedule_reconnect(uint32_t ms) {
    btstack_run_loop_remove_timer(&g.reconnect_timer);
    btstack_run_loop_set_timer_handler(&g.reconnect_timer, reconnect_timeout);
    btstack_run_loop_set_timer(&g.reconnect_timer, ms);
    btstack_run_loop_add_timer(&g.reconnect_timer);
}

void inquiry_result(const ClassicInquiryResult& r, void*) {
    if (!g.pairing || g.state != A2dpState::Pairing) return;
    std::memcpy(g.peer, r.addr, 6);
    std::memset(g.peer_name, 0, sizeof(g.peer_name));
    std::memcpy(g.peer_name, r.name, sizeof(g.peer_name) - 1);
    A2dpDevice d = device_info();
    d.rssi = r.rssi_valid ? r.rssi : 0;
    d.class_of_device = r.class_of_device;
    if (g.cfg.callback) {
        A2dpEvent e{};
        e.type = A2dpEventType::DeviceFound;
        e.device = d;
        e.volume = g.volume;
        g.cfg.callback(e, g.cfg.user);
    }
    if (g.cfg.auto_connect) {
        stop_pairing_internal();
        start_connection(r.addr);
    }
}

void a2dp_handler(uint8_t packet_type, uint16_t, uint8_t* packet, uint16_t) {
    if (packet_type != HCI_EVENT_PACKET || hci_event_packet_get_type(packet) != HCI_EVENT_A2DP_META) return;
    bd_addr_t addr;
    switch (hci_event_a2dp_meta_get_subevent_code(packet)) {
    case A2DP_SUBEVENT_SIGNALING_CONNECTION_ESTABLISHED: {
        const uint8_t status = a2dp_subevent_signaling_connection_established_get_status(packet);
        if (status != ERROR_CODE_SUCCESS) {
            g.a2dp_cid = 0;
            g.state = A2dpState::Idle;
            emit(A2dpEventType::Failed, status);
            return;
        }
        a2dp_subevent_signaling_connection_established_get_bd_addr(packet, addr);
        g.a2dp_cid = a2dp_subevent_signaling_connection_established_get_a2dp_cid(packet);
        if (!g.initiated) { // the headset called us: adopt it and open the stream ourselves
            std::memcpy(g.peer, addr, 6);
            std::memset(g.peer_name, 0, sizeof(g.peer_name));
            a2dp_source_establish_stream(g.peer, &g.a2dp_cid);
        }
        g.state = A2dpState::Connected;
        save_last();
        // AVRCP (volume) over its own channel.
        if (!g.avrcp_cid) avrcp_connect(g.peer, &g.avrcp_cid);
        emit(A2dpEventType::Connected);
        break;
    }
    case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CAPABILITY: {
        A2dpEvent e{};
        e.type = A2dpEventType::Capabilities;
        e.device = device_info();
        e.cap_rates = a2dp_subevent_signaling_media_codec_sbc_capability_get_sampling_frequency_bitmap(packet);
        e.cap_modes = a2dp_subevent_signaling_media_codec_sbc_capability_get_channel_mode_bitmap(packet);
        e.cap_blocks = a2dp_subevent_signaling_media_codec_sbc_capability_get_block_length_bitmap(packet);
        e.cap_subbands = a2dp_subevent_signaling_media_codec_sbc_capability_get_subbands_bitmap(packet);
        e.cap_alloc = a2dp_subevent_signaling_media_codec_sbc_capability_get_allocation_method_bitmap(packet);
        e.cap_min_bitpool = a2dp_subevent_signaling_media_codec_sbc_capability_get_min_bitpool_value(packet);
        e.cap_max_bitpool = a2dp_subevent_signaling_media_codec_sbc_capability_get_max_bitpool_value(packet);
        if (g.cfg.callback) g.cfg.callback(e, g.cfg.user);
        break;
    }
    case A2DP_SUBEVENT_SIGNALING_MEDIA_CODEC_SBC_CONFIGURATION: {
        if (a2dp_subevent_signaling_media_codec_sbc_configuration_get_a2dp_cid(packet) != g.a2dp_cid) return;
        g.remote_seid = a2dp_subevent_signaling_media_codec_sbc_configuration_get_remote_seid(packet);
        const uint8_t channels = a2dp_subevent_signaling_media_codec_sbc_configuration_get_num_channels(packet);
        const uint32_t rate = a2dp_subevent_signaling_media_codec_sbc_configuration_get_sampling_frequency(packet);
        const uint8_t blocks = a2dp_subevent_signaling_media_codec_sbc_configuration_get_block_length(packet);
        const uint8_t subbands = a2dp_subevent_signaling_media_codec_sbc_configuration_get_subbands(packet);
        const uint8_t min_bp = a2dp_subevent_signaling_media_codec_sbc_configuration_get_min_bitpool_value(packet);
        const uint8_t max_bp = a2dp_subevent_signaling_media_codec_sbc_configuration_get_max_bitpool_value(packet);
        const auto mode = static_cast<avdtp_channel_mode_t>(
            a2dp_subevent_signaling_media_codec_sbc_configuration_get_channel_mode(packet));
        const uint8_t alloc = a2dp_subevent_signaling_media_codec_sbc_configuration_get_allocation_method(packet);

        btstack_sbc_channel_mode_t sbc_mode = SBC_CHANNEL_MODE_JOINT_STEREO;
        A2dpChannelMode m = A2dpChannelMode::JointStereo;
        switch (mode) {
        case AVDTP_CHANNEL_MODE_MONO: sbc_mode = SBC_CHANNEL_MODE_MONO; m = A2dpChannelMode::Mono; break;
        case AVDTP_CHANNEL_MODE_DUAL_CHANNEL: sbc_mode = SBC_CHANNEL_MODE_DUAL_CHANNEL; m = A2dpChannelMode::Dual; break;
        case AVDTP_CHANNEL_MODE_STEREO: sbc_mode = SBC_CHANNEL_MODE_STEREO; m = A2dpChannelMode::Stereo; break;
        default: break;
        }
        g.active.rate = rate;
        g.active.channels = channels;
        g.active.mode = m;
        g.active.blocks = blocks;
        g.active.subbands = subbands;
        g.active.min_bitpool = min_bp;
        g.active.max_bitpool = max_bp;
        g.active.bitpool = chosen_bitpool(min_bp, max_bp);
        g.active.alloc = static_cast<btstack_sbc_allocation_method_t>(alloc - 1);
        g.enc = btstack_sbc_encoder_bluedroid_init_instance(&g.enc_state);
        g.enc->configure(&g.enc_state, SBC_MODE_STANDARD, blocks, subbands,
                         g.active.alloc, static_cast<uint16_t>(rate),
                         g.active.bitpool, sbc_mode);
        g.active.bitrate = static_cast<uint32_t>((static_cast<uint64_t>(g.enc->sbc_buffer_length(&g.enc_state)) * 8u *
                                                  rate) / g.enc->num_audio_frames(&g.enc_state));
        g.active.valid = true;
        emit(A2dpEventType::Configured);
        break;
    }
    case A2DP_SUBEVENT_STREAM_ESTABLISHED: {
        const uint8_t status = a2dp_subevent_stream_established_get_status(packet);
        if (status != ERROR_CODE_SUCCESS) {
            if (g.state != A2dpState::Idle) emit(A2dpEventType::Failed, status);
            break;
        }
        g.stream_open = true;
        if (g.cfg.auto_play) a2dp_source_start_stream(g.a2dp_cid, g.local_seid);
        break;
    }
    case A2DP_SUBEVENT_STREAM_RECONFIGURED:
        if (a2dp_subevent_stream_reconfigured_get_status(packet) == ERROR_CODE_SUCCESS)
            a2dp_source_start_stream(g.a2dp_cid, g.local_seid);
        break;
    case A2DP_SUBEVENT_STREAM_STARTED:
        g.state = A2dpState::Streaming;
        apply_volume();
        media_start();
        emit(A2dpEventType::StreamStarted);
        break;
    case A2DP_SUBEVENT_STREAMING_CAN_SEND_MEDIA_PACKET_NOW:
        if (g.streaming && g.ready_to_send) send_media_packet();
        break;
    case A2DP_SUBEVENT_STREAM_SUSPENDED:
        media_stop();
        if (g.state == A2dpState::Streaming) g.state = A2dpState::Connected;
        emit(A2dpEventType::StreamSuspended);
        break;
    case A2DP_SUBEVENT_STREAM_RELEASED:
        g.stream_open = false;
        media_stop();
        break;
    case A2DP_SUBEVENT_SIGNALING_CONNECTION_RELEASED:
        if (a2dp_subevent_signaling_connection_released_get_a2dp_cid(packet) != g.a2dp_cid) break;
        media_stop();
        g.stream_open = false;
        g.a2dp_cid = 0;
        g.avrcp_cid = 0;
        g.remote_abs = false;
        g.initiated = false;
        g.state = g.pairing ? A2dpState::Pairing : A2dpState::Idle;
        emit(A2dpEventType::Disconnected);
        if (g.restart_pending && g.have_last) { // quality change: reconnect with the new preferences
            g.restart_pending = false;
            schedule_reconnect(800);
        }
        break;
    default:
        break;
    }
}

void avrcp_handler(uint8_t packet_type, uint16_t, uint8_t* packet, uint16_t) {
    if (packet_type != HCI_EVENT_PACKET || hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    switch (packet[2]) {
    case AVRCP_SUBEVENT_CONNECTION_ESTABLISHED:
        if (avrcp_subevent_connection_established_get_status(packet) != ERROR_CODE_SUCCESS) {
            g.avrcp_cid = 0;
            return;
        }
        g.avrcp_cid = avrcp_subevent_connection_established_get_avrcp_cid(packet);
        // If the headset lets us register for volume changes it supports absolute volume.
        avrcp_controller_enable_notification(g.avrcp_cid, AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED);
        break;
    case AVRCP_SUBEVENT_CONNECTION_RELEASED:
        g.avrcp_cid = 0;
        g.remote_abs = false;
        break;
    default:
        break;
    }
}

void avrcp_controller_handler(uint8_t packet_type, uint16_t, uint8_t* packet, uint16_t) {
    if (packet_type != HCI_EVENT_PACKET || hci_event_packet_get_type(packet) != HCI_EVENT_AVRCP_META) return;
    switch (packet[2]) {
    case AVRCP_SUBEVENT_NOTIFICATION_STATE:
        if (avrcp_subevent_notification_state_get_event_id(packet) == AVRCP_NOTIFICATION_EVENT_VOLUME_CHANGED &&
            avrcp_subevent_notification_state_get_enabled(packet)) {
            g.remote_abs = true;
            apply_volume(); // push our volume to the headset
        }
        break;
    case AVRCP_SUBEVENT_NOTIFICATION_VOLUME_CHANGED:
        g.remote_abs = true;
        g.volume = avrcp_subevent_notification_volume_changed_get_absolute_volume(packet);
        emit(A2dpEventType::VolumeChanged);
        break;
    default:
        break;
    }
}

} // namespace

// ---- public free functions ----------------------------------------------------------------

A2dpSbcConfig a2dp_quality_config(A2dpQuality q) {
    switch (q) {
    case A2dpQuality::High: return {44100, A2dpChannelMode::JointStereo, 53};
    case A2dpQuality::Medium: return {44100, A2dpChannelMode::JointStereo, 35};
    case A2dpQuality::Mono: return {44100, A2dpChannelMode::Mono, 24};
    case A2dpQuality::MonoLow: return {44100, A2dpChannelMode::Mono, 16};
    default: return {44100, A2dpChannelMode::JointStereo, 25};
    }
}

const char* a2dp_quality_name(A2dpQuality q) {
    switch (q) {
    case A2dpQuality::High: return "High";
    case A2dpQuality::Medium: return "Medium";
    case A2dpQuality::Low: return "Low";
    case A2dpQuality::Mono: return "Mono";
    case A2dpQuality::MonoLow: return "MonoLow";
    default: return "Custom";
    }
}

uint32_t a2dp_sbc_bitrate_bps(uint32_t rate, A2dpChannelMode mode, uint8_t bitpool, uint8_t blocks, uint8_t subbands) {
    const uint32_t ch = mode == A2dpChannelMode::Mono ? 1 : 2;
    const uint32_t join = mode == A2dpChannelMode::JointStereo ? subbands : 0;
    uint32_t payload_bits;
    if (mode == A2dpChannelMode::Mono) payload_bits = blocks * bitpool;
    else if (mode == A2dpChannelMode::Dual) payload_bits = blocks * ch * bitpool;
    else payload_bits = join + blocks * bitpool;
    const uint32_t frame = 4 + (4 * subbands * ch) / 8 + (payload_bits + 7) / 8;
    return static_cast<uint32_t>((static_cast<uint64_t>(frame) * 8u * rate) / (static_cast<uint32_t>(blocks) * subbands));
}

// ---- BluetoothA2dp -------------------------------------------------------------------------

bool BluetoothA2dp::init(const A2dpConfig& config) {
    if (g.inited) return true;
    if (hci_get_state() != HCI_STATE_WORKING) return false; // Bluetooth::init() first
    g.cfg = config;
    g.source = config.source;
    g.quality = config.quality;
    g.preferred = resolve_quality();
    g.volume = config.initial_volume > 127 ? 127 : config.initial_volume;

    ContextLock lock;
    BluetoothClassic::init(config.pin);
    g.hci_reg.callback = &hci_handler;
    hci_add_event_handler(&g.hci_reg);

    a2dp_source_init();
    a2dp_source_register_packet_handler(&a2dp_handler);
    g.endpoint = a2dp_source_create_stream_endpoint(AVDTP_AUDIO, AVDTP_CODEC_SBC, g_sbc_capabilities,
                                                    sizeof(g_sbc_capabilities), g_sbc_configuration,
                                                    sizeof(g_sbc_configuration));
    if (!g.endpoint) return false; // not enough memory: raise MAX_NR_AVDTP_STREAM_ENDPOINTS
    g.local_seid = avdtp_local_seid(g.endpoint);
    set_preferences();

    avrcp_init();
    avrcp_register_packet_handler(&avrcp_handler);
    avrcp_controller_init();
    avrcp_controller_register_packet_handler(&avrcp_controller_handler);

    sdp_init();
    std::memset(g_sdp_a2dp_source, 0, sizeof(g_sdp_a2dp_source));
    a2dp_source_create_sdp_record(g_sdp_a2dp_source, sdp_create_service_record_handle(),
                                  AVDTP_SOURCE_FEATURE_MASK_PLAYER, nullptr, nullptr);
    sdp_register_service(g_sdp_a2dp_source);
    std::memset(g_sdp_avrcp_controller, 0, sizeof(g_sdp_avrcp_controller));
    avrcp_controller_create_sdp_record(g_sdp_avrcp_controller, sdp_create_service_record_handle(),
                                       AVRCP_FEATURE_MASK_CATEGORY_MONITOR_OR_AMPLIFIER, nullptr, nullptr);
    sdp_register_service(g_sdp_avrcp_controller);

    gap_set_local_name(config.local_name ? config.local_name : "pico-toolset");
    gap_set_class_of_device(0x200408); // audio source, as in BTstack's A2DP demo
    load_last();
    apply_volume();
    g.inited = true;
    if (config.auto_reconnect && g.have_last) schedule_reconnect(2500);
    return true;
}

void BluetoothA2dp::start_pairing() {
    if (!g.inited) return;
    ContextLock lock;
    if (g.pairing || g.state != A2dpState::Idle) return;
    g.pairing = true;
    g.state = A2dpState::Pairing;
    const uint32_t units = g.cfg.pairing_timeout_ms / 1280;
    BluetoothClassic::start_inquiry(static_cast<uint8_t>(units < 1 ? 1 : units > 48 ? 48 : units), 0x04, &inquiry_result,
                                    nullptr);
    btstack_run_loop_remove_timer(&g.pair_timer);
    btstack_run_loop_set_timer_handler(&g.pair_timer, pair_timeout);
    btstack_run_loop_set_timer(&g.pair_timer, g.cfg.pairing_timeout_ms);
    btstack_run_loop_add_timer(&g.pair_timer);
    emit(A2dpEventType::PairingStarted);
}

void BluetoothA2dp::stop_pairing() {
    if (!g.inited) return;
    ContextLock lock;
    stop_pairing_internal();
}

bool BluetoothA2dp::connect(const uint8_t addr[6]) {
    if (!g.inited) return false;
    ContextLock lock;
    if (g.pairing) stop_pairing_internal();
    return start_connection(addr);
}

bool BluetoothA2dp::connect_last() {
    if (!g.inited || !g.have_last) return false;
    ContextLock lock;
    std::memcpy(g.peer_name, g.last.name, sizeof(g.peer_name));
    if (g.pairing) stop_pairing_internal();
    return start_connection(g.last.addr);
}

void BluetoothA2dp::disconnect() {
    if (!g.inited) return;
    ContextLock lock;
    if (g.a2dp_cid) a2dp_source_disconnect(g.a2dp_cid);
}

void BluetoothA2dp::forget_last() {
    if (!g.inited) return;
    ContextLock lock;
    if (g.have_last) gap_drop_link_key_for_bd_addr(g.last.addr);
    erase_last();
    if (g.a2dp_cid) a2dp_source_disconnect(g.a2dp_cid);
}

void BluetoothA2dp::play() {
    if (!g.inited) return;
    ContextLock lock;
    if (g.a2dp_cid && g.stream_open && !g.streaming) a2dp_source_start_stream(g.a2dp_cid, g.local_seid);
}

void BluetoothA2dp::pause() {
    if (!g.inited) return;
    ContextLock lock;
    if (g.a2dp_cid && g.streaming) a2dp_source_pause_stream(g.a2dp_cid, g.local_seid);
}

void BluetoothA2dp::set_source(AudioSource* source) {
    if (!g.inited) { g.source = source; return; }
    ContextLock lock;
    g.source = source;
    if (g.streaming && source) source->prepare(g.active.rate, g.active.channels);
}

void BluetoothA2dp::set_quality(A2dpQuality quality) {
    if (!g.inited) return;
    ContextLock lock;
    g.quality = quality;
    const A2dpSbcConfig next = resolve_quality();
    const bool same_format = g.active.valid && next.sample_rate == g.active.rate && next.mode == g.active.mode;
    g.preferred = next;
    set_preferences();
    if (g.a2dp_cid && !same_format) { // different rate/mode: needs a new stream configuration
        g.restart_pending = true;
        a2dp_source_disconnect(g.a2dp_cid);
    } else {
        reconfigure_encoder_bitpool(); // bitpool only: takes effect on the next frame
    }
}

void BluetoothA2dp::set_custom(const A2dpSbcConfig& config) {
    if (!g.inited) return;
    {
        ContextLock lock;
        g.cfg.custom = config;
    }
    set_quality(A2dpQuality::Custom);
}

A2dpQuality BluetoothA2dp::quality() const { return g.quality; }

void BluetoothA2dp::set_volume(uint8_t volume) {
    if (!g.inited) return;
    ContextLock lock;
    g.volume = volume > 127 ? 127 : volume;
    apply_volume();
    emit(A2dpEventType::VolumeChanged);
}

uint8_t BluetoothA2dp::volume() const { return g.volume; }
bool BluetoothA2dp::remote_volume_active() const { return g.remote_abs && g.cfg.volume_mode != A2dpVolumeMode::Software; }
A2dpState BluetoothA2dp::state() const { return g.state; }
bool BluetoothA2dp::has_last_device() const { return g.have_last; }

A2dpDevice BluetoothA2dp::last_device() const {
    A2dpDevice d{};
    if (g.have_last) {
        std::memcpy(d.addr, g.last.addr, 6);
        std::memcpy(d.name, g.last.name, sizeof(d.name));
    }
    return d;
}

A2dpSbcConfig BluetoothA2dp::active_config() const { return {g.active.rate, g.active.mode, g.active.bitpool}; }
uint32_t BluetoothA2dp::active_bitrate_bps() const { return g.active.bitrate; }
A2dpStats BluetoothA2dp::stats() const { return g.stats; }

} // namespace pico_toolset

#endif // ENABLE_CLASSIC
