// SBC quality presets and bit-rate arithmetic: no BTstack dependency (unit-tested on a PC).
#include "pico_toolset/bluetooth_a2dp.h"

namespace pico_toolset {

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

} // namespace pico_toolset
