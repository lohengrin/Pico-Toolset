#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace pico_toolset {

// Where the store lives: `sector_count` (>= 2) consecutive 4 KiB flash sectors
// starting at `offset` (a physical flash offset, multiple of 4096). The
// application must NOT be linked over this range -- reserve it (e.g. by
// shortening the FLASH region in the linker script) and put it at the end of
// flash so it also survives a bootloader (PicoBoot) reflashing the app.
struct FlashStoreConfig {
    uint32_t offset;
    uint8_t  sector_count;

    static constexpr FlashStoreConfig at_end_of_flash(uint32_t flash_size_bytes, uint8_t sectors = 2) {
        return {flash_size_bytes - sectors * 4096u, sectors};
    }
};

// Flash access used by the store. The default (real flash) implementation is in
// flash_store_rp.cpp; host unit tests supply a RAM-backed one.
struct FlashIo {
    // Pointer to the flash content at `offset` (memory mapped / XIP).
    const uint8_t* (*read)(uint32_t offset);
    // Erases the 4 KiB sector starting at `offset`.
    bool (*erase_sector)(uint32_t offset);
    // Programs one 256-byte page at `offset` (the sector must be erased).
    bool (*program_page)(uint32_t offset, const uint8_t* page);
};

// A tiny, power-fail-safe, wear-friendly store for ONE small settings blob
// (<= kMaxPayload bytes), kept as a log: every save appends a CRC-checked
// 256-byte record to the next free page; a sector is erased only when its 16
// pages are used up, then the next sector takes over. load() returns the newest
// valid record, so an interrupted write (erase or program) can only ever lose
// that last save, never the previous settings.
//
// Intended use: save only occasionally (settings changes), and only after a
// change has settled -- erasing/programming flash briefly halts execution from
// flash (the real implementation disables interrupts, or uses
// flash_safe_execute() when core1 is a registered victim, see
// flash_store_rp.cpp -- do not use it with unregistered code running on core1).
//
// RP2350 note: reads go through XIP at the physical offset, so under
// PicoBoot's flash address translation (normal-build apps) they would not see
// the physical tail; link the app into the partition instead.
class FlashStore {
public:
    static constexpr size_t kSectorSize = 4096;
    static constexpr size_t kPageSize = 256;
    static constexpr size_t kMaxPayload = 240;

    // Scans the region for the newest valid record. Always succeeds (an empty
    // or corrupt region just has no record yet).
    bool init(const FlashStoreConfig& config, const FlashIo& io);

    // Same, on the real flash (flash_store_rp.cpp).
    bool init(const FlashStoreConfig& config);

    [[nodiscard]] bool has_record() const { return m_seq != 0; }
    [[nodiscard]] uint32_t sequence() const { return m_seq; }

    // Copies the newest record's payload into `out` and its size into `len`.
    // False if there is no record or `out` is too small.
    bool load(std::span<uint8_t> out, size_t& len) const;

    // Appends a record with `data` (<= kMaxPayload bytes) -- unless it is
    // identical to the newest one, in which case nothing is written. Returns
    // true if the data is stored afterwards.
    bool save(std::span<const uint8_t> data);

private:
    struct Header;
    [[nodiscard]] size_t page_count() const { return static_cast<size_t>(m_cfg.sector_count) * (kSectorSize / kPageSize); }
    [[nodiscard]] const uint8_t* page(size_t index) const { return m_io.read(m_cfg.offset + static_cast<uint32_t>(index * kPageSize)); }
    [[nodiscard]] bool page_erased(size_t index) const;
    [[nodiscard]] bool valid(size_t index, uint32_t& seq, size_t& len) const;
    bool pick_target(size_t& target);

    FlashStoreConfig m_cfg{};
    FlashIo m_io{};
    uint32_t m_seq = 0;       // 0 = no record
    size_t m_newest = 0;      // page index of the newest record (valid if m_seq != 0)
};

} // namespace pico_toolset
