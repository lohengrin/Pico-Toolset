#pragma once
// The Bluetooth bond store shared by all toolset applications on a board (see
// docs/components/bluetooth-store.md): BTstack's flash store with the link keys, the LE device
// database and the toolset's own device records.

#include <cstdint>

namespace pico_toolset {

struct BluetoothStoreInfo {
    uint32_t flash_offset;        // from the start of flash
    uint32_t xip_address;         // the same, as seen by the CPU (and `picotool save -r`)
    uint32_t size_bytes;          // two sectors, used alternately
    uint8_t  link_key_slots;      // Classic link keys (least recently used is evicted)
    uint8_t  device_db_slots;     // LE bonding entries
    uint8_t  format_version;      // of the toolset's own records (tags 'PHID', 'A2DP')
};

// What this build uses. The values are checked against the toolset's contract at build time
// (static_assert in bluetooth_store.cpp); two builds for the same board that both pass the check
// read and write the same store.
BluetoothStoreInfo bluetooth_store_info();

} // namespace pico_toolset
