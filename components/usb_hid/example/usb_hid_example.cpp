// PIO-USB HID host example (doubles as an integration test).
// Polls keyboard, mouse and gamepad state and prints to the serial console.
#include "pico_toolset/usb_hid_host.h"
#include "pico_toolset/usb_hid_configs.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::Keymap;
using pico_toolset::UsbHidHost;
using pico_toolset::kDefaultKeymapIndex;
using pico_toolset::kKeymapCount;
using pico_toolset::kKeymaps;
using pico_toolset::keymap_by_name;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    printf("keymaps compiled in (%u):\n", kKeymapCount);
    for (uint8_t i = 0; i < kKeymapCount; ++i)
        printf("  [%u] %s%s\n", i, kKeymaps[i].name, i == kDefaultKeymapIndex ? " (default)" : "");

    // On a different board (or a build where core1/PIO0 are already spoken
    // for by another driver), pick a different preset or copy this one and
    // adjust -- see usb_hid_configs.h.
    auto cfg = pico_toolset::configs::usb_hid::kWaveshareRp2350PiZeroLcd;
    cfg.enable_keyboard = true;
    cfg.enable_mouse = true;
    cfg.enable_gamepad = true;
    cfg.enable_xinput = true;

    // Keyboard layout for the typed-ASCII queue. The default is the build-time
    // PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP; switch to another compiled-in layout
    // at runtime by name (nullptr when not compiled in -- see
    // PICO_TOOLSET_USB_HID_KEYMAPS).
    cfg.keymap_index = kDefaultKeymapIndex;
    if (const Keymap* fr = keymap_by_name("fr"))
        cfg.keymap_index = static_cast<uint8_t>(fr - kKeymaps);
    printf("USB HID host starting (keymap #%u: %s)...\n",
           cfg.keymap_index, kKeymaps[cfg.keymap_index].name);
    UsbHidHost usb;
    usb.init(cfg);

    while (true) {
        // The host stack runs on core1 (run_on_core1); this loop only polls
        // the double-buffered state. No tuh_task() needed here.

        if (usb.connected_keyboard_count() > 0) {
            // Edge-triggered typed characters, in cfg.keymap_index's layout.
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