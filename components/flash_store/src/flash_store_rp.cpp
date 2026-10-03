// FlashStore on the real flash of an RP2040/RP2350.
#include "pico_toolset/flash_store.h"

#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/flash.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"

namespace pico_toolset {

namespace {

// Erase/program disable XIP, so nothing may execute from flash meanwhile:
//  * if the other core registered as a flash_safe_execute() victim
//    (multicore_lockout_ready()), use flash_safe_execute(), which parks it;
//  * otherwise the other core is assumed not to be running (the usual
//    single-core firmware) and disabling interrupts on this core is enough.
//    Calling flash_safe_execute() in that case would refuse (or assert in
//    debug builds) when multicore lockout support is compiled in.
// Same policy as pico_toolset_psram's init -- see this repo's AGENTS.md. A
// firmware running unregistered code on core1 must not use this store.
constexpr uint32_t kSafeExecuteTimeoutMs = 1000;

struct Op {
    uint32_t offset;
    const uint8_t* data; // null: erase a sector
};

void do_op(void* param) {
    const Op* op = static_cast<const Op*>(param);
    if (op->data == nullptr)
        flash_range_erase(op->offset, FLASH_SECTOR_SIZE);
    else
        flash_range_program(op->offset, op->data, FLASH_PAGE_SIZE);
}

bool run(const Op& op) {
    if (multicore_lockout_ready())
        return flash_safe_execute(do_op, const_cast<Op*>(&op), kSafeExecuteTimeoutMs) == PICO_OK;

    const uint32_t irq = save_and_disable_interrupts();
    do_op(const_cast<Op*>(&op));
    restore_interrupts(irq);
    return true;
}

const uint8_t* rp_read(uint32_t offset) { return reinterpret_cast<const uint8_t*>(XIP_BASE + offset); }
bool rp_erase(uint32_t offset) { return run({offset, nullptr}); }
bool rp_program(uint32_t offset, const uint8_t* page) { return run({offset, page}); }

} // namespace

bool FlashStore::init(const FlashStoreConfig& config) {
    return init(config, FlashIo{rp_read, rp_erase, rp_program});
}

} // namespace pico_toolset
