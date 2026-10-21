// BLE HID smoke test: pairs keyboards, mice and gamepads (e.g. an Xbox
// wireless controller) over Bluetooth LE, remembers them across reboots and
// reconnects them. On boot it reconnects bonded devices; for a new one put the
// device in pairing mode and press 'p' on the serial console (or it starts
// pairing automatically when nothing is bonded). 'l' lists bonded devices,
// 'f' forgets them all. Needs a CYW43 board (PICO_BOARD=pimoroni_presto or
// pico2_w).
#include <cstdio>

#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico_toolset/bluetooth.h"
#include "pico_toolset/bluetooth_hid.h"

using namespace pico_toolset;

static const char* kind_str(uint8_t k) {
    static char buf[24];
    std::snprintf(buf, sizeof(buf), "%s%s%s", k & bt_hid_kind::Keyboard ? "kbd " : "",
                  k & bt_hid_kind::Mouse ? "mouse " : "", k & bt_hid_kind::Gamepad ? "pad" : "");
    return buf;
}

static void on_event(const BtHidEvent& e, void*) {
    const auto& d = e.device;
    switch (e.type) {
    case BtHidEventType::PairingStarted: std::printf("[pairing] scanning...\n"); break;
    case BtHidEventType::PairingStopped: std::printf("[pairing] stopped\n"); break;
    case BtHidEventType::DeviceFound:
        std::printf("[found] %02X:%02X:%02X:%02X:%02X:%02X '%s' rssi %d\n", d.addr[0], d.addr[1],
                    d.addr[2], d.addr[3], d.addr[4], d.addr[5], d.name, d.rssi);
        break;
    case BtHidEventType::Connecting: std::printf("[connecting] '%s'\n", d.name); break;
    case BtHidEventType::PasskeyDisplay:
        std::printf("[passkey] type %06lu on the keyboard, then Enter\n", static_cast<unsigned long>(e.passkey));
        break;
    case BtHidEventType::Paired: std::printf("[paired] '%s'\n", d.name); break;
    case BtHidEventType::Connected: std::printf("[connected] '%s' (%s)\n", d.name, kind_str(d.kinds)); break;
    case BtHidEventType::Disconnected: std::printf("[disconnected] '%s'\n", d.name); break;
    case BtHidEventType::PairingFailed:
        std::printf("[failed] '%s' status 0x%02x reason 0x%02x\n", d.name, e.status, e.reason);
        break;
    case BtHidEventType::Forgotten: std::printf("[forgotten] '%s'\n", d.name); break;
    }
}

int main() {
    stdio_init_all();
    sleep_ms(2000);
    if (cyw43_arch_init()) return 1;

    Bluetooth bt;
    if (!bt.init({})) { std::printf("bluetooth init failed\n"); return 1; }

    BluetoothHid hid;
    BluetoothHidConfig cfg;
    cfg.callback = on_event;
    if (!hid.init(cfg)) { std::printf("bluetooth_hid init failed\n"); return 1; }

    BtHidDevice bonded[BluetoothHid::kMaxBonded];
    size_t n = hid.bonded_devices(bonded, BluetoothHid::kMaxBonded);
    std::printf("%u bonded device(s)\n", static_cast<unsigned>(n));
    if (n == 0) hid.start_pairing();

    GamepadState prev_pad{};
    while (true) {
        const int c = getchar_timeout_us(0);
        if (c == 'p') hid.start_pairing();
        else if (c == 'f') hid.forget_all();
        else if (c == 'l') {
            n = hid.bonded_devices(bonded, BluetoothHid::kMaxBonded);
            for (size_t i = 0; i < n; ++i)
                std::printf("  '%s' %s %s\n", bonded[i].name, kind_str(bonded[i].kinds),
                            bonded[i].connected ? "(connected)" : "");
        }

        for (int usage = 4; usage < 0x68; ++usage)
            if (hid.consume_key_press(static_cast<uint8_t>(usage))) std::printf("key 0x%02X\n", usage);

        int dx, dy;
        hid.consume_mouse_delta(dx, dy);
        if (dx || dy) std::printf("mouse %d,%d\n", dx, dy);

        for (size_t i = 0; i < BluetoothHid::kMaxConnected; ++i) {
            const GamepadState g = hid.gamepad_state(i);
            if (g.present && (g.buttons != prev_pad.buttons || (g.lx >> 4) != (prev_pad.lx >> 4) ||
                              (g.ly >> 4) != (prev_pad.ly >> 4))) {
                std::printf("pad%u btn 0x%04X raw 0x%08lX L(%u,%u) R(%u,%u) LT %u RT %u\n",
                            static_cast<unsigned>(i), g.buttons,
                            static_cast<unsigned long>(hid.gamepad_raw_buttons(i)), g.lx, g.ly, g.rx,
                            g.ry, g.lt, g.rt);
                prev_pad = g;
            }
        }
        sleep_ms(5);
    }
}
