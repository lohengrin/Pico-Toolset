# bluetooth

BLE broadcaster and observer (non-connectable advertising plus scanning) over BTstack on the CYW43 radio. No GATT.

| | |
|---|---|
| Target / option | `pico_toolset_bluetooth` / `PICO_TOOLSET_BUILD_BLUETOOTH` (**OFF**, needs a CYW43 board) |
| Links | INTERFACE library with sources: `pico_cyw43_arch_lwip_threadsafe_background`, `pico_btstack_ble`, `pico_btstack_cyw43`, `pico_toolset_net_config` |
| Example | `components/bluetooth/example/bluetooth_example.cpp` (built when `pico_toolset_https_client` exists) |
| Boards | [Pimoroni Presto](../boards/pimoroni-presto.md) (also Pico W / Pico 2 W) |

## API

- `BluetoothConfig { name = "pico-toolset"; adv_interval_ms = 100; scan_interval_ms = 30; active_scan = true }` -- behavior only; the CYW43 wiring comes from the board header.
- `bool init(cfg)` -- brings BTstack up and waits (up to 5 s) for the controller. **`cyw43_arch_init()` must already have run**: `wifi_connect()` does that, or call it yourself.
- `start_advertising()` / `stop_advertising()` -- ADV_NONCONN_IND carrying flags and the local name.
- `start_scan(callback, user)` / `stop_scan()` -- `BleAdvertisement { addr, addr_type, rssi, name }` per report. The callback runs on the radio's async context, not an interrupt; keep it short.

## Build notes

- The build has **one** `btstack_config.h` (`components/bluetooth/config/`): LE central + peripheral roles, pools sized for three HID devices; an `#ifdef ENABLE_CLASSIC` block adds the Classic limits. Classic support is opt-in with the CMake option `PICO_TOOLSET_BLUETOOTH_CLASSIC` (see [bluetooth_hid](bluetooth-hid.md#bluetooth-classic-older-pads-and-keyboards)).
- Pool-size macros use this BTstack version's names (e.g. `MAX_NR_HIDS_HOSTS`); a pool of 0 shows up as "Not enough memory" in the log.
- The shared `lwipopts.h` from `pico_toolset_net_config` enables lwIP's TLS layer, so the final executable must also link mbedtls: link `pico_toolset_https_client` and include `cmake/pico_toolset_tls_config.cmake` before `pico_sdk_init()` (the toolset's top-level does that when `PICO_TOOLSET_BUILD_BLUETOOTH` or `..._HTTPS_CLIENT` is ON).
- Like `wifi`, the component is an INTERFACE library so the SDK's stack sources compile once, in the executable.
- BTstack, lwIP and mbedtls together need roughly 85 KB of SRAM; with a 480x480 Presto framebuffer the link overflows RAM.
- **Bluetooth only (no Wi-Fi):** configure with `-DPICO_TOOLSET_BLUETOOTH_NO_WIFI=ON` (or `set()` it before the toolset is added) to link `pico_cyw43_arch_none` instead of the lwIP arch: no lwIP, no mbedtls, no `net_config`/`https_client`. A BLE+Classic HID host build then adds only ~18 KB of `.bss` (measured, `bluetooth_hid_example` with Classic on). The in-tree `bluetooth*_example` targets are skipped in this mode (they need `https_client`). Call `cyw43_arch_init()` yourself before `Bluetooth::init()`.
