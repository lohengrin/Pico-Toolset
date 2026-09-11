// Reset-buttons example (doubles as an integration test).
// Three buttons, each rebooting with a different tag -- a real consumer
// would check consume_pending_watchdog_tag() early in main() to pick a
// startup mode based on which button triggered the reboot.
#include "pico_toolset/reset_buttons.h"
#include "pico/stdlib.h"

#include <array>
#include <cstdio>

using pico_toolset::DebouncedButtons;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    uint32_t pending_tag = 0;
    if (pico_toolset::consume_pending_watchdog_tag(pending_tag)) {
        printf("Rebooted with tag %u\n", pending_tag);
    } else {
        printf("Normal power-on\n");
    }

    // Adapt pins here for your board.
    constexpr std::array<uint8_t, 3> kPins = {14, 15, 16};
    DebouncedButtons buttons;
    buttons.init(kPins);

    while (true) {
        int pressed = buttons.poll();
        if (pressed >= 0) {
            printf("Button %d debounced -- rebooting with tag %d\n", pressed, pressed);
            pico_toolset::watchdog_reboot_with_tag(static_cast<uint32_t>(pressed));
        }
        sleep_ms(20);
    }
}
