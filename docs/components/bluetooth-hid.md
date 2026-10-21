# bluetooth_hid

BLE HID-over-GATT **host**: pairs, bonds and reconnects Bluetooth LE keyboards, mice and
gamepads (e.g. Xbox wireless controllers) and exposes them through the same kind of state
accessors as [`usb_hid`](usb-hid.md).

> **Status:** pairing, bonding and typing validated on the Presto with a Microsoft Universal
> Foldable Keyboard (legacy LE pairing, passkey entry). Gamepads and mice are written but not
> yet tested on hardware; the gamepad button map in particular is unverified. Bluetooth
> Classic HID (DualShock 4, DualSense, Switch Pro, old keyboards) is not implemented yet.

| | |
|---|---|
| Target / option | `pico_toolset_bluetooth_hid` / `PICO_TOOLSET_BUILD_BLUETOOTH_HID` (**OFF**, needs `PICO_TOOLSET_BUILD_BLUETOOTH`) |
| Links | INTERFACE library with sources: `pico_toolset_bluetooth`, `pico_toolset_hid_input`, `pico_btstack_flash_bank` |
| Example | `components/bluetooth_hid/example/bluetooth_hid_example.cpp`; integrated in [`examples/presto`](../boards/pimoroni-presto.md) |
| Boards | [Pimoroni Presto](../boards/pimoroni-presto.md) (also Pico W / Pico 2 W) |
| Shared types | `GamepadState` / `kBt*` live in `components/hid_input` (`usb_hid_gamepad.h` forwards to it) |

## Using it

```cpp
cyw43_arch_init();
Bluetooth bt;  bt.init({});            // brings the controller up
BluetoothHid hid;
BluetoothHidConfig cfg; cfg.callback = on_event;
hid.init(cfg);                          // loads bonded devices, reconnects them
if (no devices bonded) hid.start_pairing();   // new device: put it in pairing mode
// then poll hid.is_key_down(), hid.gamepad_state(0), hid.consume_mouse_delta() ...
```

`BluetoothHidConfig`: `callback`/`user`, `auto_reconnect` (true), `auto_pair` (true: pair the
first HID device seen), `pairing_timeout_ms` (30 s). The callback runs in the BTstack
context, not an interrupt and not your thread: only record state in it.

## Pairing

1. `start_pairing()` scans (active) for advertisements carrying the HID service (UUID 0x1812)
   or a HID GAP appearance (0x03C0-0x03CF). The device must itself be in pairing mode.
2. `DeviceFound` is reported per device; with `auto_pair` the first one is connected,
   otherwise call `pair(addr, type)`.
3. The Security Manager pairs with *Display only* I/O capability, bonding, MITM protection and
   LE Secure Connections (many keyboards refuse plain Just Works / legacy pairing): mice and
   gamepads use Just Works; **a keyboard triggers `PasskeyDisplay` -- show the 6-digit code and
   type it on that keyboard, then Enter.**
4. `Paired` -> the HID service is discovered -> `Connected` (`device.kinds` says keyboard /
   mouse / gamepad, from the report descriptor).

Events: `PairingStarted`, `PairingStopped`, `DeviceFound`, `Connecting`, `PasskeyDisplay`,
`Paired`, `Connected`, `Disconnected`, `PairingFailed` (`status` = BTstack/HCI error, `reason` = SM reason code when `status` is 0x05),
`Forgotten`.

## Remembering devices

- **Keys:** BTstack stores bonds in its TLV store in flash. The pico-sdk's CYW43 BTstack port
  sets that up (and the LE device database) by itself.
- **Device records:** this component also stores address, kind and name of up to 4 bonded
  devices (TLV tag `PHID`), which is what `bonded_devices()` lists and what reconnect uses.
- **Reconnect:** after `init()` and after each disconnect, bonded devices are added to the
  controller's whitelist and a whitelist connection is started, so devices reconnect when
  switched on, with no scanning. Starting pairing pauses it.
- `forget(addr)` / `forget_all()` disconnect, remove the whitelist entry and the bond, and
  delete the record. If a device lost its side of the bond (reset, paired elsewhere) the
  re-encryption fails and the stale bond is deleted automatically: pair it again.
- Up to 3 devices connected at once (`kMaxConnected`), 4 bonded (`kMaxBonded`).

## Input accessors

| | |
|---|---|
| Keyboard | `is_key_down(usage)`, `is_modifier_down(mask)`, `consume_key_press(usage)` -- HID usage IDs (see `usb_hid_keymap.h` for ASCII conversion) |
| Mouse | `mouse_state()`, `consume_mouse_delta(dx, dy)`, `consume_mouse_wheel()` |
| Gamepad | `gamepad_state(i)` (`GamepadState`: buttons `kBt*`, `lx/ly/rx/ry/lt/rt` 0-255, centred at 128), `gamepad_raw_buttons(i)` |
| Counts | `connected_keyboard_count()`, `connected_mouse_count()`, `connected_gamepad_count()` |

Reports are parsed generically from the device's HID descriptor (BTstack's HID parser).
Gamepad mapping: X/Y = left stick, Z/Rz = right stick, Simulation Brake/Accelerator = LT/RT, hat =
D-pad, buttons by HID usage: 1 A, 2 B, 4 X, 5 Y, 7 LB, 8 RB, 11 Back, 12 Start, 13 Guide,
14 LS, 15 RS (the Linux order Xbox controllers use over Bluetooth). **This table is the part
most likely to need adjusting per controller**: `gamepad_raw_buttons()` returns the raw usages
so you can see what your pad sends.

## Flash safety (read before using bonding with two cores)

Storing a bond erases/programs flash from the BTstack context. During that window nothing may
execute from flash:

- Core 1 (if used) must run only from RAM. In `examples/presto` core 1 serves the LCD scanout
  interrupts (RAM-resident) and idles in a `__not_in_flash_func` loop.
- Tell the SDK that core 1 is safe: define **`PICO_FLASH_ASSUME_CORE1_SAFE=1`**. Without it
  `flash_safe_execute()` cannot lock out an unregistered core and the bond write asserts.
  (The toolset's rule against registering a flash-lockout victim on core 1 is why this is the
  chosen mechanism.)
- **Flash region collision:** BTstack's bank is two sectors near the end of flash (on RP2350:
  three sectors from the end). [`flash_store`](flash-store.md) also takes the last sectors by
  default. If you use both, move one with `PICO_FLASH_BANK_STORAGE_OFFSET` /
  `PICO_FLASH_BANK_TOTAL_SIZE`, or shorten `flash_store`'s reserve.

## Troubleshooting pairing

- `PairingFailed` with `status 0x05 reason 0x03` means *we* refused the device's pairing
  parameters. BTstack enables "Secure Connections only" by default when SC is compiled in;
  `BluetoothHid::init()` switches that off so legacy-pairing devices (many keyboards) work.
- `status 0x13` means the device hung up (often after receiving a pairing failure).
- For the exact exchange, build the Presto example with `-DPRESTO_BT_HCI_DUMP=ON`: the packet
  log starts when a connection begins and shows the SMP pairing request/response packets
  (SMP is L2CAP channel 6: opcode 01 request, 02 response, 05 failed, 0B security request).

- `status 0x56` from the HID connect (log: "Not enough memory to create client") means the
  BTstack pool `MAX_NR_HIDS_HOSTS` (see `components/bluetooth/config/btstack_config.h`) is too
  small.

## Build notes

- Same BTstack/lwIP/mbedtls requirements as [`bluetooth`](bluetooth.md); the shared
  `btstack_config.h` sizes allow 3 simultaneous HID connections.
- `Bluetooth::start_scan()` and HID pairing share the single GAP scan: do not run both.
- BLE only: Classic HID would add `ENABLE_CLASSIC`, `hid_host` and link-key storage (planned).
- No HID output yet (keyboard LEDs, rumble).
