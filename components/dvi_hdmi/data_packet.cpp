/*
 * See data_packet.h's doc comment for provenance (ported from Shuichi
 * Takano's dvi::DataPacket, MIT License) and the wire format this
 * implements.
 *
 * HDMI data-island wire format in one paragraph: each data island is a
 * fixed-size run of TERC4-encoded (Transition-Minimized 4-bit) symbols on
 * all three TMDS lanes, bracketed by 2-symbol guardbands. Lane 0 carries a
 * repeating 4-bit header value derived from the current vsync/hsync state;
 * lanes 1/2 carry the packet's header bytes (BCH(31,26)-parity-protected,
 * 4 bytes) followed by its four 8-byte subpackets (each independently
 * BCH-protected), bit-interleaved two nibbles at a time per makeTERC4x2Char().
 * This file only encodes; the framing (where in a scanline's blanking
 * interval this stream gets DMA'd out) is dvi_timing.c's job.
 */
#include "data_packet.h"

#include <cstring>

namespace {

// BCH(31,26) parity generator, table-driven (one table lookup per input
// byte, XORed together) -- the standard technique for this polynomial.
// Verbatim from the upstream port; this table is derived from the CEA-861
// spec's BCH generator polynomial, not something to hand-tune.
//
// `const` + TOM6809_DATA_PACKET_RAM_DATA rather than `constexpr`: constexpr
// would pin this in flash .rodata, and data_packet_set_audio_sample() reads
// it up to 35 times per scanline from the core1 DMA IRQ (see data_packet.h's
// RAM residency note -- this exact table sitting in flash was half of a
// confirmed real-hardware picture loss).
// clang-format off
const uint8_t TOM6809_DATA_PACKET_RAM_DATA kBchTable[256] = {
    0x00, 0xd9, 0xb5, 0x6c, 0x6d, 0xb4, 0xd8, 0x01,
    0xda, 0x03, 0x6f, 0xb6, 0xb7, 0x6e, 0x02, 0xdb,
    0xb3, 0x6a, 0x06, 0xdf, 0xde, 0x07, 0x6b, 0xb2,
    0x69, 0xb0, 0xdc, 0x05, 0x04, 0xdd, 0xb1, 0x68,
    0x61, 0xb8, 0xd4, 0x0d, 0x0c, 0xd5, 0xb9, 0x60,
    0xbb, 0x62, 0x0e, 0xd7, 0xd6, 0x0f, 0x63, 0xba,
    0xd2, 0x0b, 0x67, 0xbe, 0xbf, 0x66, 0x0a, 0xd3,
    0x08, 0xd1, 0xbd, 0x64, 0x65, 0xbc, 0xd0, 0x09,
    0xc2, 0x1b, 0x77, 0xae, 0xaf, 0x76, 0x1a, 0xc3,
    0x18, 0xc1, 0xad, 0x74, 0x75, 0xac, 0xc0, 0x19,
    0x71, 0xa8, 0xc4, 0x1d, 0x1c, 0xc5, 0xa9, 0x70,
    0xab, 0x72, 0x1e, 0xc7, 0xc6, 0x1f, 0x73, 0xaa,
    0xa3, 0x7a, 0x16, 0xcf, 0xce, 0x17, 0x7b, 0xa2,
    0x79, 0xa0, 0xcc, 0x15, 0x14, 0xcd, 0xa1, 0x78,
    0x10, 0xc9, 0xa5, 0x7c, 0x7d, 0xa4, 0xc8, 0x11,
    0xca, 0x13, 0x7f, 0xa6, 0xa7, 0x7e, 0x12, 0xcb,
    0x83, 0x5a, 0x36, 0xef, 0xee, 0x37, 0x5b, 0x82,
    0x59, 0x80, 0xec, 0x35, 0x34, 0xed, 0x81, 0x58,
    0x30, 0xe9, 0x85, 0x5c, 0x5d, 0x84, 0xe8, 0x31,
    0xea, 0x33, 0x5f, 0x86, 0x87, 0x5e, 0x32, 0xeb,
    0xe2, 0x3b, 0x57, 0x8e, 0x8f, 0x56, 0x3a, 0xe3,
    0x38, 0xe1, 0x8d, 0x54, 0x55, 0x8c, 0xe0, 0x39,
    0x51, 0x88, 0xe4, 0x3d, 0x3c, 0xe5, 0x89, 0x50,
    0x8b, 0x52, 0x3e, 0xe7, 0xe6, 0x3f, 0x53, 0x8a,
    0x41, 0x98, 0xf4, 0x2d, 0x2c, 0xf5, 0x99, 0x40,
    0x9b, 0x42, 0x2e, 0xf7, 0xf6, 0x2f, 0x43, 0x9a,
    0xf2, 0x2b, 0x47, 0x9e, 0x9f, 0x46, 0x2a, 0xf3,
    0x28, 0xf1, 0x9d, 0x44, 0x45, 0x9c, 0xf0, 0x29,
    0x20, 0xf9, 0x95, 0x4c, 0x4d, 0x94, 0xf8, 0x21,
    0xfa, 0x23, 0x4f, 0x96, 0x97, 0x4e, 0x22, 0xfb,
    0x93, 0x4a, 0x26, 0xff, 0xfe, 0x27, 0x4b, 0x92,
    0x49, 0x90, 0xfc, 0x25, 0x24, 0xfd, 0x91, 0x48,
};
// clang-format on

// These helpers are all on the per-scanline core1 IRQ path (via
// data_packet_set_audio_sample()/data_packet_encode()) and so are marked
// RAM-resident for the case where the compiler chooses not to inline them --
// see data_packet.h's RAM residency note.
uint8_t TOM6809_DATA_PACKET_FUNC(encode_bch3)(const uint8_t *p) {
    uint8_t v = kBchTable[p[0]];
    v = kBchTable[p[1] ^ v];
    v = kBchTable[p[2] ^ v];
    return v;
}

uint8_t TOM6809_DATA_PACKET_FUNC(encode_bch7)(const uint8_t *p) {
    uint8_t v = kBchTable[p[0]];
    for (int i = 1; i < 7; ++i) {
        v = kBchTable[p[i] ^ v];
    }
    return v;
}

// Even-parity of a byte's set bits, folded manually rather than via a
// lookup table -- this is only ever called on up to 3 bytes per audio
// sub-packet (24 bytes/scanline at most), nowhere near hot enough to need
// the table the upstream C++ used for the same computation.
uint8_t TOM6809_DATA_PACKET_FUNC(byte_parity)(uint8_t v) {
    v ^= static_cast<uint8_t>(v >> 4);
    v ^= static_cast<uint8_t>(v >> 2);
    v ^= static_cast<uint8_t>(v >> 1);
    return v & 1;
}

uint8_t TOM6809_DATA_PACKET_FUNC(parity3)(uint8_t a, uint8_t b, uint8_t c) {
    return static_cast<uint8_t>(byte_parity(a) ^ byte_parity(b) ^ byte_parity(c));
}

// TERC4 (Transition-Minimized 4-bit) symbol table -- CEA-861's fixed
// 4-bit-to-10-bit code, indexed by nibble value.
//
// RAM-resident for the same reason as kBchTable above, and more urgently:
// data_packet_encode() indexes this ~70 times per scanline, every scanline,
// from the core1 DMA IRQ.
const uint16_t TOM6809_DATA_PACKET_RAM_DATA kTerc4Syms[16] = {
    0b1010011100, 0b1001100011, 0b1011100100, 0b1011100010, 0b0101110001, 0b0100011110, 0b0110001110, 0b0100111100,
    0b1011001100, 0b0100111001, 0b0110011100, 0b1011000110, 0b1010001110, 0b1001110001, 0b0101100011, 0b1011000011,
};

uint32_t TOM6809_DATA_PACKET_FUNC(make_terc4_x2)(int i0, int i1) {
    return static_cast<uint32_t>(kTerc4Syms[i0]) | (static_cast<uint32_t>(kTerc4Syms[i1]) << 10);
}
uint32_t TOM6809_DATA_PACKET_FUNC(make_terc4_x2)(int i) { return make_terc4_x2(i, i); }

// Data-island guardband symbol (lanes 1/2), fixed per spec -- two copies of
// the 10-bit pattern 0100110011 (0x133) packed into one 20-bit word:
// 0x133 | (0x133 << 10) = 0x4CD33.
constexpr uint32_t kDataGuardbandSym = 0x4CD33u;

void TOM6809_DATA_PACKET_FUNC(compute_header_parity)(data_packet_t *packet) {
    packet->header[3] = encode_bch3(packet->header);
}

void TOM6809_DATA_PACKET_FUNC(compute_subpacket_parity)(data_packet_t *packet, int i) {
    packet->subpacket[i][7] = encode_bch7(packet->subpacket[i]);
}

void compute_info_frame_checksum(data_packet_t *packet) {
    int s = 0;
    for (int i = 0; i < 3; ++i) {
        s += packet->header[i];
    }
    int n = packet->header[2] + 1;
    for (int j = 0; j < 4 && n > 0; ++j) {
        for (int i = 0; i < 7 && n > 0; ++i, --n) {
            s += packet->subpacket[j][i];
        }
    }
    packet->subpacket[0][0] = static_cast<uint8_t>(-s);
}

} // namespace

void data_packet_compute_parity(data_packet_t *packet) {
    compute_header_parity(packet);
    for (int i = 0; i < 4; ++i) {
        compute_subpacket_parity(packet, i);
    }
}

void TOM6809_DATA_PACKET_FUNC(data_packet_set_null)(data_packet_t *packet) { std::memset(packet, 0, sizeof(*packet)); }

void data_packet_set_avi_info_frame(data_packet_t *packet, scan_info_t scan, pixel_format_t pixel_format,
                                     colorimetry_t colorimetry, picture_aspect_ratio_t picture_aspect_ratio,
                                     active_format_aspect_ratio_t active_format_aspect_ratio,
                                     rgb_quantization_range_t rgb_quantization_range, video_code_t video_code) {
    data_packet_set_null(packet);
    packet->header[0] = 0x82;
    packet->header[1] = 2;  // version
    packet->header[2] = 13; // length

    const int sc = 0; // no known scaling
    packet->subpacket[0][1] = static_cast<uint8_t>(
        static_cast<int>(scan) | (active_format_aspect_ratio == ACTIVE_FORMAT_ASPECT_RATIO_NO_DATA ? 0 : 16) |
        (static_cast<int>(pixel_format) << 5));
    packet->subpacket[0][2] = static_cast<uint8_t>(static_cast<int>(picture_aspect_ratio) |
                                                    (static_cast<int>(active_format_aspect_ratio) << 4) |
                                                    (static_cast<int>(colorimetry) << 6));
    packet->subpacket[0][3] = static_cast<uint8_t>(sc | (static_cast<int>(rgb_quantization_range) << 2));
    packet->subpacket[0][4] = static_cast<uint8_t>(video_code);

    compute_info_frame_checksum(packet);
    data_packet_compute_parity(packet);
}

void data_packet_set_audio_info_frame(data_packet_t *packet, int freq) {
    data_packet_set_null(packet);
    packet->header[0] = 0x84;
    packet->header[1] = 1;  // version
    packet->header[2] = 10; // length

    const int cc = 1; // 2 channels
    const int ct = 1; // IEC 60958 PCM
    const int ss = 1; // 16-bit
    const int sf = freq == 48000 ? 3 : (freq == 44100 ? 2 : 0);
    const int ca = 0; // FL, FR
    const int lsv = 0;
    const int dm_inh = 0;
    packet->subpacket[0][1] = static_cast<uint8_t>(cc | (ct << 4));
    packet->subpacket[0][2] = static_cast<uint8_t>(ss | (sf << 2));
    packet->subpacket[0][4] = static_cast<uint8_t>(ca);
    packet->subpacket[0][5] = static_cast<uint8_t>((lsv << 3) | (dm_inh << 7));

    compute_info_frame_checksum(packet);
    data_packet_compute_parity(packet);
}

void data_packet_set_audio_clock_regeneration(data_packet_t *packet, int cts, int n) {
    packet->header[0] = 1;
    packet->header[1] = 0;
    packet->header[2] = 0;
    compute_header_parity(packet);

    uint8_t *sp0 = packet->subpacket[0];
    sp0[0] = 0;
    sp0[1] = static_cast<uint8_t>(cts >> 16);
    sp0[2] = static_cast<uint8_t>(cts >> 8);
    sp0[3] = static_cast<uint8_t>(cts);
    sp0[4] = static_cast<uint8_t>(n >> 16);
    sp0[5] = static_cast<uint8_t>(n >> 8);
    sp0[6] = static_cast<uint8_t>(n);
    compute_subpacket_parity(packet, 0);

    // Subpackets 1-3 are identical to subpacket 0 for this packet type
    // (CEA-861 allows carrying up to 4 independent ACR values per packet;
    // this driver only ever advertises one).
    std::memcpy(packet->subpacket[1], sp0, 8);
    std::memcpy(packet->subpacket[2], sp0, 8);
    std::memcpy(packet->subpacket[3], sp0, 8);
}

int TOM6809_DATA_PACKET_FUNC(data_packet_set_audio_sample)(data_packet_t *packet, audio_ring_t *ring, int n,
                                                            int frame_ct) {
    const int layout = 0; // 2-channel layout
    const int sample_present = (1 << n) - 1;
    const int b = frame_ct < 4 ? (1 << frame_ct) : 0;
    packet->header[0] = 2;
    packet->header[1] = static_cast<uint8_t>((layout << 4) | sample_present);
    packet->header[2] = static_cast<uint8_t>(b << 4);
    compute_header_parity(packet);

    const uint32_t mask = ring->size - 1;
    const uint32_t rp = ring->read;
    for (int i = 0; i < n; ++i) {
        const audio_sample_t &s = ring->buffer[(rp + static_cast<uint32_t>(i)) & mask];
        const int16_t l = s.channels[0];
        const int16_t r = s.channels[1];
        uint8_t *d = packet->subpacket[i];
        d[0] = 0;
        d[1] = static_cast<uint8_t>(l);
        d[2] = static_cast<uint8_t>(l >> 8);
        d[3] = 0;
        d[4] = static_cast<uint8_t>(r);
        d[5] = static_cast<uint8_t>(r >> 8);
        constexpr uint8_t kValid = 1; // IEC 60958 "V" (validity) bit, per channel
        const uint8_t pl = parity3(d[1], d[2], kValid);
        const uint8_t pr = parity3(d[4], d[5], kValid);
        d[6] = static_cast<uint8_t>((kValid << 0) | (pl << 3) | (kValid << 4) | (pr << 7));
        compute_subpacket_parity(packet, i);
    }
    for (int i = n; i < 4; ++i) {
        std::memset(packet->subpacket[i], 0, 8);
    }
    audio_ring_advance_read(ring, static_cast<uint32_t>(n));

    frame_ct -= n;
    if (frame_ct < 0) {
        frame_ct += 192; // IEC 60958 channel-status block is 192 frames long
    }
    return frame_ct;
}

void TOM6809_DATA_PACKET_FUNC(data_packet_encode)(data_island_stream_t *stream, const data_packet_t *packet,
                                                   bool vsync, bool hsync) {
    const int hv = (vsync ? 2 : 0) | (hsync ? 1 : 0);
    const int hv1 = hv | 8;

    stream->data[0][0] = make_terc4_x2(0b1100 | hv);
    stream->data[1][0] = kDataGuardbandSym;
    stream->data[2][0] = kDataGuardbandSym;

    // Header: rides all three lanes for 16 words (4 header bytes x 4 words
    // each). Every symbol's low nibble carries the running hv/hv1 vsync/
    // hsync marker (constant per spec once past the very first symbol);
    // the high nibble carries 2 bits of the actual header byte per word,
    // low-to-high across the 4 words for a given byte.
    {
        uint32_t *dst = &stream->data[0][1];
        int cur_hv = hv;
        for (int i = 0; i < 4; ++i) {
            const uint8_t h = packet->header[i];
            dst[0] = make_terc4_x2(((h << 2) & 4) | cur_hv, ((h << 1) & 4) | hv1);
            dst[1] = make_terc4_x2((h & 4) | hv1, ((h >> 1) & 4) | hv1);
            dst[2] = make_terc4_x2(((h >> 2) & 4) | hv1, ((h >> 3) & 4) | hv1);
            dst[3] = make_terc4_x2(((h >> 4) & 4) | hv1, ((h >> 5) & 4) | hv1);
            dst += 4;
            cur_hv = hv1;
        }
    }

    // Subpackets, bit-interleaved 4 lanes-worth-of-bytes at a time across
    // lanes 1/2 (8 output words each) -- see the bit-interleave comment at
    // the top of this file.
    {
        uint32_t *dst1 = &stream->data[1][1];
        uint32_t *dst2 = &stream->data[2][1];
        for (int i = 0; i < 8; ++i) {
            uint32_t v = static_cast<uint32_t>(packet->subpacket[0][i]) | (static_cast<uint32_t>(packet->subpacket[1][i]) << 8) |
                         (static_cast<uint32_t>(packet->subpacket[2][i]) << 16) |
                         (static_cast<uint32_t>(packet->subpacket[3][i]) << 24);
            uint32_t t = (v ^ (v >> 7)) & 0x00aa00aau;
            v = v ^ t ^ (t << 7);
            t = (v ^ (v >> 14)) & 0x0000ccccu;
            v = v ^ t ^ (t << 14);
            dst1[0] = make_terc4_x2(static_cast<int>((v >> 0) & 15), static_cast<int>((v >> 16) & 15));
            dst1[1] = make_terc4_x2(static_cast<int>((v >> 4) & 15), static_cast<int>((v >> 20) & 15));
            dst2[0] = make_terc4_x2(static_cast<int>((v >> 8) & 15), static_cast<int>((v >> 24) & 15));
            dst2[1] = make_terc4_x2(static_cast<int>((v >> 12) & 15), static_cast<int>((v >> 28) & 15));
            dst1 += 2;
            dst2 += 2;
        }
    }

    stream->data[0][N_DATA_ISLAND_WORDS - 1] = make_terc4_x2(0b1100 | hv);
    stream->data[1][N_DATA_ISLAND_WORDS - 1] = kDataGuardbandSym;
    stream->data[2][N_DATA_ISLAND_WORDS - 1] = kDataGuardbandSym;
}

const uint32_t *data_packet_get_default_island_12(void) {
    static uint32_t table[N_DATA_ISLAND_WORDS];
    static bool initialized = false;
    if (!initialized) {
        table[0] = kDataGuardbandSym;
        for (int i = 1; i < N_DATA_ISLAND_WORDS - 1; ++i) {
            table[i] = make_terc4_x2(0);
        }
        table[N_DATA_ISLAND_WORDS - 1] = kDataGuardbandSym;
        initialized = true;
    }
    return table;
}

const uint32_t *data_packet_get_default_island_0(bool vsync, bool hsync) {
    static uint32_t table[4][N_DATA_ISLAND_WORDS];
    static bool initialized[4] = {false, false, false, false};
    const int idx = (vsync ? 2 : 0) | (hsync ? 1 : 0);
    if (!initialized[idx]) {
        table[idx][0] = make_terc4_x2(0b1100 | idx);
        table[idx][1] = make_terc4_x2(0b0000 | idx);
        for (int i = 2; i < N_DATA_ISLAND_WORDS - 1; ++i) {
            table[idx][i] = make_terc4_x2(0b1000 | idx);
        }
        table[idx][N_DATA_ISLAND_WORDS - 1] = make_terc4_x2(0b1100 | idx);
        initialized[idx] = true;
    }
    return table[idx];
}
