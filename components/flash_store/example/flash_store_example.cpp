// FlashStore example (doubles as an integration test): counts reboots in the
// last two flash sectors. Power-cycle the board and watch the counter on the
// USB serial port. NOTE: it uses the last 8 KB of flash -- fine for this tiny
// example, but a real application must reserve that range in its linker script.
#include "pico_toolset/flash_store.h"
#include "pico/stdlib.h"

#include <cstdio>
#include <cstring>

int main() {
    stdio_init_all();
    sleep_ms(2000);

    pico_toolset::FlashStore store;
    store.init(pico_toolset::FlashStoreConfig::at_end_of_flash(PICO_FLASH_SIZE_BYTES));

    uint32_t boots = 0;
    uint8_t buf[pico_toolset::FlashStore::kMaxPayload];
    size_t len = 0;
    if (store.load(buf, len) && len == sizeof boots)
        std::memcpy(&boots, buf, sizeof boots);
    printf("record seq=%u, boots so far: %u\n", store.sequence(), boots);

    ++boots;
    bool ok = store.save({reinterpret_cast<const uint8_t*>(&boots), sizeof boots});
    printf("saved %u: %s (seq=%u)\n", boots, ok ? "ok" : "FAILED", store.sequence());

    while (true) sleep_ms(1000);
}
