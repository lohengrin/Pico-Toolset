#include "pico_toolset/bluetooth_store.h"

#include "btstack_config.h"
#include "hardware/flash.h"
#include "pico/btstack_flash_bank.h"

// The contract that makes bonds portable between applications. A build that deliberately uses
// another layout (and so does not share bonds) defines PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT.
#ifndef PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT

#ifndef PICO_TOOLSET_BT_NVM_LINK_KEYS
#error "btstack_config.h is not the toolset's (components/bluetooth/config): bonds would not be shared. Use it, or define PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT"
#endif

static_assert(PICO_FLASH_BANK_TOTAL_SIZE == 2 * FLASH_SECTOR_SIZE,
              "bond store must be the SDK default two sectors (or define PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT)");
#if PICO_RP2350 && PICO_RP2350_A2_SUPPORTED
static_assert(PICO_FLASH_BANK_STORAGE_OFFSET == PICO_FLASH_SIZE_BYTES - 3 * FLASH_SECTOR_SIZE,
              "bond store must be at the SDK default offset (or define PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT)");
#else
static_assert(PICO_FLASH_BANK_STORAGE_OFFSET == PICO_FLASH_SIZE_BYTES - 2 * FLASH_SECTOR_SIZE,
              "bond store must be at the SDK default offset (or define PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT)");
#endif
static_assert(NVM_NUM_LINK_KEYS == PICO_TOOLSET_BT_NVM_LINK_KEYS, "NVM_NUM_LINK_KEYS differs from the toolset's");
static_assert(NVM_NUM_DEVICE_DB_ENTRIES == PICO_TOOLSET_BT_NVM_DEVICE_DB,
              "NVM_NUM_DEVICE_DB_ENTRIES differs from the toolset's");

#endif // PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT

namespace pico_toolset {

BluetoothStoreInfo bluetooth_store_info() {
    BluetoothStoreInfo i{};
    i.flash_offset = PICO_FLASH_BANK_STORAGE_OFFSET;
    i.xip_address = XIP_BASE + PICO_FLASH_BANK_STORAGE_OFFSET;
    i.size_bytes = PICO_FLASH_BANK_TOTAL_SIZE;
    i.link_key_slots = NVM_NUM_LINK_KEYS;
    i.device_db_slots = NVM_NUM_DEVICE_DB_ENTRIES;
    i.format_version = 1;
    return i;
}

} // namespace pico_toolset
