// PIO-USB HID host example (doubles as an integration test).
// Polls keyboard, mouse and gamepad state and prints to the serial console.
#include "pico_toolset/usb_hid_host.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::UsbHidConfig;
using pico_toolset::UsbHidHost;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    UsbHidConfig cfg;
    cfg.pin_dp = 28;          // default Pico-PIO-USB D+ pin (D- is pin 29)
    cfg.run_on_core1 = true;  // dedicate core1 to the host stack
    cfg.enable_keyboard = true;
    cfg.enable_mouse = true;
    cfg.enable_gamepad = true;
    cfg.enable_xinput = true;

    UsbHidHost usb;
    printf("USB HID host starting...\n");
    usb.init(cfg);

    while (true) {
        // The host stack runs on core1 (run_on_core1); this loop only polls
        // the double-buffered state. No tuh_task() needed here.

        if (usb.connected_keyboard_count() > 0) {
            // Edge-triggered typed characters (US layout).
            uint8_t ch;
            while ((ch = usb.consume_typed_ascii_char()) != 0)
                putchar(ch);

            // Level-triggered keys.
            if (usb.is_key_down(0x28)) // HID usage 0x28 = Enter
                printf("<ENTER>\n");
        }

        if (usb.connected_gamepad_count() > 0) {
            auto g = usb.gamepad_state(0);
            if (g.present) {
                printf("pad0: btns=%04X lx=%u ly=%u lt=%u rt=%u\n", g.buttons, g.lx, g.ly, g.lt, g.rt);
            }
        }

        auto m = usb.mouse_state();
        if (m.present) {
            printf("mouse: x=%d y=%d L=%d R=%d\n", m.x, m.y, m.left_button, m.right_button);
        }

        sleep_ms(20);
    }
}