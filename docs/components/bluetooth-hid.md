# bluetooth_hid

Bluetooth HID **host**: pairs, bonds and reconnects Bluetooth LE (HID-over-GATT) and, when
built in, Bluetooth Classic keyboards, mice and gamepads, and exposes them through the same
kind of state accessors as [`usb_hid`](usb-hid.md).

> **Status (validated on the Presto):**
> - LE: pairing, bonding, typing and reconnect with a Microsoft Universal Foldable Keyboard
>   (legacy LE pairing, passkey entry).
> - Classic: pairing (legacy PIN), bonding, input and reconnect after a reset with an "SFR
>   Gamepad" (vendor 0x2E2C, class 0x2508), through the raw L2CAP fallback below.
>
> **Not yet tested:** LE gamepads (e.g. Xbox wireless controllers), mice, Classic keyboards,
> and BTstack's own Classic HID host path (no tested device had a usable SDP record). The
> gamepad button map (`kButtonMap`) is a guess from the Linux/Xbox order; check it with
> `gamepad_raw_buttons()` for your pad.
>
> **Missing:** HID output (keyboard LEDs, rumble), sixaxis/DualShock 3. The Presto example has a
> touch device manager (list / forget / pair) built on `bonded_devices()`, `forget()` and
> `start_pairing()`; PAIR NEW and the audio page behind it are used on hardware, forgetting a
> connected device is not specifically tested. Works together with `bluetooth_a2dp` (keyboard +
> gamepad + headset validated at once). Debugging help:
> [Bluetooth pairing diagnostics](../guides/bluetooth-pairing-diagnostics.md).

| | |
|---|---|
| Target / option | `pico_toolset_bluetooth_hid` / `PICO_TOOLSET_BUILD_BLUETOOTH_HID` (**OFF**, needs `PICO_TOOLSET_BUILD_BLUETOOTH`) |
| Links | INTERFACE library with sources: `pico_toolset_bluetooth`, `pico_toolset_hid_input`, `pico_btstack_flash_bank` (+ `pico_btstack_classic` with `PICO_TOOLSET_BLUETOOTH_CLASSIC`) |
| Example | `components/bluetooth_hid/example/bluetooth_hid_example.cpp`; integrated in [`examples/presto`](../boards/pimoroni-presto.md) |
| Boards | [Pimoroni Presto](../boards/pimoroni-presto.md) (also Pico W / Pico 2 W) |
| Shared types | `GamepadState` / `kBt*` live in `components/hid_input` (`usb_hid_gamepad.h` forwards to it) |

Code layout: `src/bluetooth_hid.cpp` holds the LE and Classic connection logic; descriptor
classification and report parsing live in `src/hid_report.{h,cpp}` (no BTstack stack state; covered by
the PC tests in `tests/host/`, which build BTstack's own HID parser). A further split of the LE,
Classic and raw-link parts into separate files is possible but not done.

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

## Bluetooth Classic (older pads and keyboards)

Devices that are not BLE (older gamepads, DualShock 4 / DualSense, Switch Pro, many keyboards)
use Bluetooth Classic HID. It is opt-in because it adds flash and RAM: configure with
`-DPICO_TOOLSET_BLUETOOTH_CLASSIC=ON` (the Presto example: `-DPRESTO_BT_CLASSIC=ON`, the
default). `BluetoothHidConfig::enable_classic` (true) then turns it on at run time.

- Pairing mode also runs a Classic **inquiry** and makes the board discoverable, so a pad in
  pairing mode either shows up in the inquiry (only devices whose class is "peripheral" are
  reported) or finds and connects to us itself. With `auto_pair` the first device found, LE or
  Classic, is connected.
- **Legacy PIN pairing** (older pads) is answered with `BluetoothHidConfig::pin` (default
  `"0000"`); Secure Simple Pairing is accepted (just works / numeric comparison), and a keyboard
  that shows a passkey triggers `PasskeyDisplay`.
- A discovered Classic device is first **paired and bonded** (BTstack "dedicated bonding"), and the
  HID connection is opened afterwards: some pads only reveal their full HID SDP record on an
  authenticated link and otherwise fail with status `0x11`.
- **Pads with a broken SDP server:** some old pads answer SDP with a stub record (just the service
  class), so BTstack's HID host cannot find the HID channels and reports status `0x11` even
  after pairing. For those, `bluetooth_hid` falls back to connecting to the standard HID control
  (PSM 0x11) and interrupt (PSM 0x13) channels itself and hands every input report, unparsed, to
  `BluetoothHidConfig::raw_report_callback`. The device is reported as a gamepad. Because there
  is no descriptor to read, a built-in one is assumed: the Android-style layout of the "SFR
  Gamepad" (vendor 0x2E2C; report id 3 = 15 buttons, hat, X/Y/Z/Rz, brake, accelerator), so
  such pads appear in `gamepad_state()` like any other. A pad with a different layout needs its
  own descriptor (or parsing of the raw reports in the callback).
  Such a pad is flagged in the stored device record. When it later connects *to us* (the pad
  starts the L2CAP connection after a button press) the HID host is told to accept it in
  boot-protocol mode, which makes BTstack skip the SDP query and take both of the pad's channels
  as they come; the reports are then parsed with the same built-in descriptor. With the normal
  accept, BTstack queries the stub record first, refuses the pad's interrupt channel meanwhile, and
  this pad sends its reports on the channel it was refused (HCI dump: reports arrive on a CID we
  never opened, nothing reaches the app). The first connection of a not-yet-flagged pad still goes
  through the failing SDP once, which sets the flag.
- The HID report descriptor is read over SDP, so keyboard / mouse / gamepad are detected the
  same way as for LE, and the same accessors apply.
- **Link keys and device records** are stored in flash like the LE bonds (same flash-safety rules).
  Classic devices have no address type; they are recorded with `addr_type 0xFE`.
- **Reconnect:** a paired pad reconnects by itself when you switch it on (the board stays
  connectable). The board does not page bonded Classic devices on its own.
- Classic names are usually empty (only reported if the device sends them in its inquiry data).
- Not supported: sixaxis/DualShock 3 (needs a proprietary activation), HID output (LEDs,
  rumble), a Classic device that needs a PIN other than the configured one.

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
  interrupts (RAM-resident) and idles in a `__no_inline_not_in_flash_func` loop. Keep the
  `no_inline`: with plain `__not_in_flash_func` the compiler inlined the idle loop into the
  flash-resident entry function, and core 1 died (LCD scanout stopped, black screen) when core 0
  wrote flash (bonding) while core 1 was fetching instructions from there.
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
- Classic HID needs `PICO_TOOLSET_BLUETOOTH_CLASSIC` (see above); the BTstack limits for it are
  in `components/bluetooth/config/btstack_config.h` (`#ifdef ENABLE_CLASSIC` block).
- No HID output yet (keyboard LEDs, rumble).
