# Bluetooth bond store

Where paired devices are kept, and the rules that make them portable: a device paired in one toolset
application is usable in another one built for the same board, with no re-pairing.

| | |
|---|---|
| Header | `pico_toolset/bluetooth_store.h` (`bluetooth_store_info()`) |
| Provided by | `pico_toolset_bluetooth` (it links the SDK's `pico_btstack_flash_bank`), checked in `components/bluetooth/src/bluetooth_store.cpp` |
| Used by | `bluetooth_hid`, `bluetooth_a2dp` |

## Layout

Everything lives in BTstack's flash store: a log-structured TLV (tag / length / value) in **two flash
sectors (8 KB) used alternately**, outside the firmware image, so reflashing with UF2 does not erase it.
The location is the SDK default and follows the board's flash size:

| Chip | Offset |
|---|---|
| RP2350 (A2 stepping) | `PICO_FLASH_SIZE_BYTES - 3 sectors` (the last sector stays free for erratum RP2350-E10) |
| RP2040 | `PICO_FLASH_SIZE_BYTES - 2 sectors` |

On the 16 MB Presto: flash offset `0xFFD000`, CPU address `0x10FFD000`; `bluetooth_store_info()` returns
it and the Presto example prints it at start-up.

| Tag | Owner | Content |
|---|---|---|
| `BTL` + index (8 slots) | BTstack | Classic link keys, least recently used evicted |
| `BTD` + index (8 slots) | BTstack | LE device database: address, LTK, IRK, ... |
| (local identity keys) | BTstack | created on first use |
| `PHID` | toolset | the HID device list: up to 4 records of address, type, kinds (+ raw-link flag), name |
| `A2DP` | toolset | the last headset: address and name |

## The contract

Two builds share bonds when all of this holds; the first three are **checked at build time**
(`static_assert` / `#error` in `bluetooth_store.cpp`):

1. Bank size = 2 sectors and offset = the SDK default for the board (no `PICO_FLASH_BANK_*` overrides).
2. `NVM_NUM_LINK_KEYS` = 8 and `NVM_NUM_DEVICE_DB_ENTRIES` = 8, from the toolset's
   `components/bluetooth/config/btstack_config.h` (one per build). An application with its own
   `btstack_config.h` must define the same `PICO_TOOLSET_BT_NVM_*` values.
3. The firmware image stays below the store (the SDK asserts this at start-up).
4. Same board, i.e. the same `PICO_FLASH_SIZE_BYTES`: building for a 4 MB board and running on a
   16 MB one puts the store somewhere else.
5. Same record formats for `PHID` and `A2DP` (`bluetooth_store_info().format_version`, today 1).
6. No other component overlaps the region (`flash_store` has its own sectors, see
   [bluetooth-hid.md](bluetooth-hid.md)).

An application that deliberately uses another layout (and so does not share bonds) defines
`PICO_TOOLSET_BT_STORE_CUSTOM_LAYOUT` to switch the build-time check off.

Writes happen from the BTstack context through `flash_safe_execute()`: an application that runs code on
core 1 must follow the rules in [bluetooth-hid.md](bluetooth-hid.md) (RAM-only core 1 and
`PICO_FLASH_ASSUME_CORE1_SAFE=1`). This now applies to `bluetooth_a2dp` applications as well; they
persist their bonds too.

## Things to know

- **Forgetting is global.** `BluetoothHid::forget()` and `BluetoothA2dp::forget_last()` remove the keys
  for every application.
- **No encryption.** Link keys are readable by any firmware (and by anyone who can read the flash).
- **Two records can disagree.** BTstack evicts keys by recency while `PHID` evicts by age and has fewer
  slots (4 against 8): a device can be listed without its key, or keyed without being listed (for
  example when it was paired by an application that does not use the toolset). Re-pair it in that case.
  A single registry that fixes this is planned (phase 2 of the shared-store proposal).

## Inspecting and clearing it

With `picotool` (adjust the address for your board, see `bluetooth_store_info()`):

```sh
picotool save -r 0x10FFD000 0x10FFF000 bt-store.bin   # dump
picotool erase -r 0x10FFD000 0x10FFF000               # forget every bond of every application
```
