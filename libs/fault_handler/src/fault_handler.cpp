#include "pico_toolset/fault_handler.h"

// First version of this handler tried to print immediately and force the
// bytes out over USB CDC by calling tud_task() in a loop (HardFault runs at
// the highest exception priority, so the IRQ stdio_usb normally relies on
// can't preempt it to finish the transfer). That version shipped, was
// tested on hardware, and produced *no output* on a reproducible freeze --
// which, without more information, reads as "no fault happened here
// either." Attaching a debug probe and breaking in with GDB found out what
// was actually going on: a hard fault WAS happening every time, the handler
// WAS being entered, but its own tud_task() flush loop was itself
// deadlocking -- calling back into TinyUSB's device-stack task function
// from inside an asynchronous exception context re-enters state (queues,
// spinlock-guarded critical sections shared with another core's host stack)
// that can legitimately be mid-update at the exact instant a fault
// interrupts normal execution, and the fault handler's re-entrant call then
// blocked forever waiting on that same lock. So the very mechanism meant to
// make faults visible was itself hanging silently, indistinguishable from
// "no fault occurred".
//
// Fixed by not touching USB (or anything else with cross-core/interrupt
// state) from fault context at all: stash the diagnostic registers in the
// watchdog's scratch registers (plain 32-bit words that -- unlike ordinary
// SRAM -- are guaranteed to survive a watchdog reset), then reboot via
// watchdog_reboot(). The *next* boot, with stdio/USB fully and normally
// initialized (no exception context, no reentrancy risk), checks those
// scratch registers and reports the stashed fault before the app's own
// logic starts -- see report_pending_hard_fault(), meant to be called early
// from main().

#include <cstdio>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/structs/watchdog.h"

namespace pico_toolset {

namespace {

// Cortex-M33 System Control Block fault status/address registers -- same
// addresses/layout as every other Cortex-M with the optional fault
// registers implemented (M3/M4/M33).
volatile uint32_t* const kCFSR = reinterpret_cast<volatile uint32_t*>(0xE000ED28);

// Arbitrary marker distinguishing "a fault stashed this" from scratch
// registers' power-on-reset contents (undefined) or a previous boot's
// leftovers after consume_pending_hard_fault() has already cleared it.
// Distinct from reset_buttons' kRebootMagic (different scratch slot too --
// see fault_handler.h's allocation table).
constexpr uint32_t kFaultMagic = 0xFA17FA17;

} // namespace

extern "C" void hard_fault_handler_c(uint32_t* stacked)
{
    // Exception entry auto-stacks these 8 words, in this order, on
    // whichever stack (MSP/PSP) was active -- the isr_hardfault trampoline
    // below figures out which and passes that pointer here.
    uint32_t pc = stacked[6];
    uint32_t lr = stacked[5];

    watchdog_hw->scratch[3] = pc;
    watchdog_hw->scratch[5] = lr;
    watchdog_hw->scratch[6] = *kCFSR;
    watchdog_hw->scratch[2] = kFaultMagic;

    // Same mechanism a project's own I_Quit()-style clean reboot would use --
    // confirmed on hardware to actually reboot the board over a plain USB
    // CDC connection, no debug probe needed.
    watchdog_reboot(0, 0, 10);
    while (true)
        tight_loop_contents(); // watchdog_reboot() only arms the reset; wait for it to fire.
}

// Naked trampoline: the C calling convention needs the stacked-registers
// pointer in r0, but which stack pointer (MSP or PSP) that is depends on
// bit 2 of the EXC_RETURN value the CPU puts in LR on exception entry --
// only asm can read that before the (nonexistent, we're naked) C prologue
// would clobber it. Standard, widely-used ARM Cortex-M hard-fault-handler
// pattern -- see e.g. ARM's own application notes on the topic.
extern "C" __attribute__((naked)) void isr_hardfault(void)
{
    __asm volatile(
        "movs r0, #4        \n"
        "mov  r1, lr        \n"
        "tst  r0, r1        \n"
        "beq  1f            \n"
        "mrs  r0, psp       \n"
        "b    2f            \n"
        "1:                 \n"
        "mrs  r0, msp       \n"
        "2:                 \n"
        "b    hard_fault_handler_c \n"
    );
}

bool consume_pending_hard_fault(FaultInfo& out)
{
    if (watchdog_hw->scratch[2] != kFaultMagic)
        return false;

    out.pc = watchdog_hw->scratch[3];
    out.lr = watchdog_hw->scratch[5];
    out.cfsr = watchdog_hw->scratch[6];
    watchdog_hw->scratch[2] = 0; // reported -- don't re-report on the next normal boot
    return true;
}

bool report_pending_hard_fault(const char* tag)
{
    FaultInfo info{};
    if (!consume_pending_hard_fault(info))
        return false;

    printf("\n*** PREVIOUS BOOT ENDED IN A HARD FAULT ***\n");
    printf("%s: PC=0x%08lx LR=0x%08lx CFSR=0x%08lx\n",
           tag ? tag : "PicoToolset",
           (unsigned long)info.pc, (unsigned long)info.lr, (unsigned long)info.cfsr);
    printf("%s: PC is the faulting instruction's address -- look it up\n"
           "%s: in your .elf.map or via arm-none-eabi-addr2line.\n",
           tag ? tag : "PicoToolset", tag ? tag : "PicoToolset");
    return true;
}

} // namespace pico_toolset
