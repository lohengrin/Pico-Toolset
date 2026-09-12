// Fault-handler example (doubles as an integration test): reports any fault
// from the *previous* boot, then deliberately triggers one after a delay so
// you can watch the "crash -> reboot -> report" cycle over a serial
// terminal without a debug probe attached.
#include "pico_toolset/fault_handler.h"
#include "pico/stdlib.h"

#include <cstdio>

int main() {
    stdio_init_all();
    sleep_ms(2000); // let a USB-CDC terminal attach before we print

    if (!pico_toolset::report_pending_hard_fault("FaultHandlerExample")) {
        printf("Normal power-on -- no fault pending.\n");
    }

    printf("Crashing in 3 seconds (writing through a null pointer)...\n");
    sleep_ms(3000);

    volatile int* bad = nullptr;
    *bad = 42; // triggers a hard fault; isr_hardfault stashes it and reboots
    while (true) tight_loop_contents();
}
