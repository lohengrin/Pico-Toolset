# Bluetooth page (`libs/bluetooth_ui`)

A device page any application can drop in: pair new devices (HID or audio), list the bonded ones, show
their name and connection state, connect, disconnect and forget them. It lives in `libs/` because the
view uses `libs/screen`; it only depends on components.

> **Status: phase 1 of 5.** The platform-independent logic is written and unit-tested on a PC. The
> on-screen view, the input adaptors and the Presto integration are not written yet, so there is
> nothing to see on a display so far, and nothing here has run on hardware.

## Layers

| Layer | Where | Status |
|---|---|---|
| Backends: what a component exposes | `bluetooth_ui_hid.h`, `bluetooth_ui_a2dp.h` (header-only adaptors over `BluetoothHid` / `BluetoothA2dp`) | compiled, not run on hardware |
| Model: merged list and commands | `BluetoothDevices` | host-tested |
| Controller: the page's state machine | `BluetoothPageController` | host-tested |
| View, sized to the screen | `libs/screen` `DisplayDriver` | phase 2 |
| Input adaptors (touch, pointer, keyboard, pad, buttons) | | phase 3 |

## Model

`BluetoothDevices` takes one `BtUiBackend` per component (`add_backend`). It merges `bonded()` and
`found()` across them, hides found devices that are already bonded or that answered two scans, and
`start_scan()` scans every backend at once. `change_counter()` is the sum of the backends' counters
plus its own, so a page only redraws when something moved.

The HID adaptor has no `disconnect` (HID devices reconnect by themselves), so the detail screen only
offers Connect or Forget for them; the audio adaptor offers Disconnect while connected.

## Controller

Screens: **List** (bonded devices, Pair new, Close), **Scan** (found devices, Stop/Start, Back),
**Detail** (Connect or Disconnect, Forget, Back) and **ConfirmForget** (Cancel is focused first).

The controller never draws. It exposes a flat list of *items* (`item(i)` = action + device index): the
view draws one control per item wherever it fits and sends input back as `BtUiInput`
(`Prev`/`Next`/`Select`/`Back`) or `activate(item)` for a tap. A device picked in Scan is awaited:
once it shows up bonded the page returns to the list with `BtUiNotice::Paired`; if the backend
reports an error it stays on Scan with `PairFailed` and the status code. A keyboard's passkey is
available from `passkey()` while it is pairing.

## What the components gained for this

`BluetoothHid` and `BluetoothA2dp` now expose, without any callback:

- `change_counter()`, bumped on every event;
- `found_devices()`, the devices seen by the current scan (HID also with `auto_pair` on);
- HID: `connecting()`, `pairing_passkey()`, `last_pairing_error()`; A2DP: `current_device()`,
  `last_error()`.

For the user to choose what to pair, configure the components with `auto_pair = false` (HID) and
`auto_connect = false` (A2DP).

## Tests

`tests/host/test_bluetooth_ui.cpp` drives the model and controller with fake backends (merge and
de-duplication, navigation and clamped focus, detail and forget with the safe default, external forget,
pairing success and failure, passkey, scan stop on leaving, focus kept across list changes). Run them
with the command in AGENTS.md.
