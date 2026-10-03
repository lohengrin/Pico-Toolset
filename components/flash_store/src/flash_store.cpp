// Portable part of FlashStore (no Pico SDK): the record log logic.
#include "pico_toolset/flash_store.h"

#include <cstring>

namespace pico_toolset {

namespace {

constexpr uint32_t kMagic = 0x53465450; // "PTFS"
constexpr size_t kHeaderSize = 16;      // magic, seq, len(u16), reserved(u16), crc

uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return crc;
}

uint32_t read_u32(const uint8_t* p) { uint32_t v; std::memcpy(&v, p, 4); return v; }
uint16_t read_u16(const uint8_t* p) { uint16_t v; std::memcpy(&v, p, 2); return v; }

// Checksum over seq + len + payload (the magic is checked separately)
uint32_t record_crc(uint32_t seq, uint16_t len, const uint8_t* payload) {
    uint8_t head[6];
    std::memcpy(head, &seq, 4);
    std::memcpy(head + 4, &len, 2);
    return ~crc32(payload, len, crc32(head, sizeof head));
}

// Sequence numbers wrap: a is newer than b if the signed difference is positive
bool newer(uint32_t a, uint32_t b) { return static_cast<int32_t>(a - b) > 0; }

} // namespace

bool FlashStore::page_erased(size_t index) const {
    const uint8_t* p = page(index);
    for (size_t i = 0; i < kPageSize; ++i)
        if (p[i] != 0xFF) return false;
    return true;
}

bool FlashStore::valid(size_t index, uint32_t& seq, size_t& len) const {
    const uint8_t* p = page(index);
    if (read_u32(p) != kMagic) return false;
    seq = read_u32(p + 4);
    const uint16_t l = read_u16(p + 8);
    if (seq == 0 || l > kMaxPayload) return false;
    if (read_u32(p + 12) != record_crc(seq, l, p + kHeaderSize)) return false;
    len = l;
    return true;
}

bool FlashStore::init(const FlashStoreConfig& config, const FlashIo& io) {
    m_cfg = config;
    m_io = io;
    m_seq = 0;
    m_newest = 0;
    for (size_t i = 0; i < page_count(); ++i) {
        uint32_t seq; size_t len;
        if (valid(i, seq, len) && (m_seq == 0 || newer(seq, m_seq))) {
            m_seq = seq;
            m_newest = i;
        }
    }
    return true;
}

bool FlashStore::load(std::span<uint8_t> out, size_t& len) const {
    if (m_seq == 0) return false;
    uint32_t seq; size_t n;
    if (!valid(m_newest, seq, n) || n > out.size()) return false;
    std::memcpy(out.data(), page(m_newest) + kHeaderSize, n);
    len = n;
    return true;
}

// Chooses (and prepares) the page the next record goes to: the first fully
// erased page after the newest record in its sector; when the sector is full,
// erase the next sector and start at its first page. With no record yet, the
// first sector is used (erased first if it is not already clean).
bool FlashStore::pick_target(size_t& target) {
    constexpr size_t kPagesPerSector = kSectorSize / kPageSize;
    size_t sector = m_seq == 0 ? 0 : m_newest / kPagesPerSector;

    if (m_seq != 0) {
        for (size_t p = m_newest + 1; p < (sector + 1) * kPagesPerSector; ++p) {
            if (page_erased(p)) { target = p; return true; }
        }
        sector = (sector + 1) % m_cfg.sector_count;   // sector full (or unusable pages): rotate
    } else {
        size_t first_free = kPagesPerSector;
        for (size_t p = 0; p < kPagesPerSector; ++p)
            if (page_erased(p)) { first_free = p; break; }
        if (first_free < kPagesPerSector) { target = first_free; return true; }
    }

    if (!m_io.erase_sector(m_cfg.offset + static_cast<uint32_t>(sector * kSectorSize)))
        return false;
    target = sector * kPagesPerSector;
    return true;
}

bool FlashStore::save(std::span<const uint8_t> data) {
    if (data.size() > kMaxPayload) return false;

    if (m_seq != 0) {                                 // identical to the newest record: nothing to do
        uint8_t current[kMaxPayload];
        size_t n;
        if (load(current, n) && n == data.size() && std::memcmp(current, data.data(), n) == 0)
            return true;
    }

    size_t target;
    if (!pick_target(target)) return false;

    const uint32_t seq = m_seq + 1 == 0 ? 1 : m_seq + 1;   // never 0 (0 = "no record")
    uint8_t buf[kPageSize];
    std::memset(buf, 0xFF, sizeof buf);
    const uint16_t len = static_cast<uint16_t>(data.size());
    const uint16_t reserved = 0;
    const uint32_t magic = kMagic;
    std::memcpy(buf, &magic, 4);
    std::memcpy(buf + 4, &seq, 4);
    std::memcpy(buf + 8, &len, 2);
    std::memcpy(buf + 10, &reserved, 2);
    std::memcpy(buf + kHeaderSize, data.data(), data.size());
    const uint32_t crc = record_crc(seq, len, data.data());
    std::memcpy(buf + 12, &crc, 4);

    if (!m_io.program_page(m_cfg.offset + static_cast<uint32_t>(target * kPageSize), buf))
        return false;

    uint32_t check_seq; size_t check_len;
    if (!valid(target, check_seq, check_len) || check_seq != seq) // read back
        return false;

    m_seq = seq;
    m_newest = target;
    return true;
}

} // namespace pico_toolset
