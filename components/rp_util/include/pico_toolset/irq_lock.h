#pragma once

#include "hardware/sync.h"

namespace pico_toolset {

// RAII interrupt-off section on the calling core: keep it to a few microseconds. Use it for
// state shared with an interrupt handler or an async-context worker (e.g. the Bluetooth stack
// callbacks, which preempt thread code on the same core). It does not protect against the
// other core.
class IrqLock {
public:
    IrqLock() : m_flags(save_and_disable_interrupts()) {}
    ~IrqLock() { restore_interrupts(m_flags); }
    IrqLock(const IrqLock&) = delete;
    IrqLock& operator=(const IrqLock&) = delete;

private:
    uint32_t m_flags;
};

} // namespace pico_toolset
