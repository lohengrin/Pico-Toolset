# Bluetooth page (`libs/bluetooth_ui`)

A device page any application can drop in: pair new devices (HID or audio), list the bonded ones, show
their name and connection state, connect, disconnect and forget them. It lives in `libs/` because the
view uses `libs/screen`; it only depends on components.

> **Status: phase 4 of 5, plus the LVGL view.** The logic, the layout, the view and the input are written and unit-tested on
> a PC (the view renders into a fake display, input comes from fakes), and the Presto example now uses
> the page instead of its own device menu (it builds; **not yet run on the board**).

## Layers

| Layer | Where | Status |
|---|---|---|
| Backends: what a component exposes | `bluetooth_ui_hid.h`, `bluetooth_ui_a2dp.h` (header-only adaptors over `BluetoothHid` / `BluetoothA2dp`) | compiled, not run on hardware |
| Model: merged list and commands | `BluetoothDevices` | host-tested |
| Controller: the page's state machine | `BluetoothPageController` | host-tested |
| Layout: where everything goes, hit-testing | `bluetooth_page_layout.h` (pure arithmetic) | host-tested |
| View, drawn on any `DisplayDriver` | `BluetoothPageView` | rendered in host tests, not on hardware |
| Input: touch, mouse, keyboard, pad, buttons | `BluetoothPageInput` | host-tested with a fake `HidSource` |
| Words shared by every view | `bluetooth_page_text.h` (titles, status, labels, row and detail text) | host-tested through both views |
| **Alternative view, drawn with LVGL** | `libs/bluetooth_ui_lvgl`: `BluetoothPageLvgl` | host-tested on a headless LVGL; running in PicoBoot (Presto) |

## Choosing a view

The logic (model, controller, texts) is the same for both; an application picks the view that matches
how it draws:

| | `BluetoothPageView` (+ `BluetoothPageInput`) | `BluetoothPageLvgl` |
|---|---|---|
| For | applications that draw on a `DisplayDriver` (libs/screen): OLED, e-paper, ST77xx, Presto without LVGL | applications that already run LVGL |
| Link | `pico_toolset_bluetooth_ui` (with `pico_toolset_screen` present) | `pico_toolset_bluetooth_ui_lvgl` (needs the `lvgl` target; it brings the logic) |
| Input | `BluetoothPageInput` polls a `HidSource`, touch and buttons | LVGL's own: touch / pointer indevs click the objects, a keypad / encoder indev drives the page's `lv_group` |
| Look | the 5x8 font scaled to the area, theme `BtUiTheme` | the application's LVGL theme and fonts (`BluetoothPageLvglConfig`) |
| Screens, actions, texts | List, Scan, Detail, ConfirmForget | the same |

```cpp
BluetoothPageController controller(devices);
BluetoothPageLvgl page(controller, {.title = LV_SYMBOL_BLUETOOTH " Bluetooth",
                                    .title_font = &lv_font_montserrat_28, .body_font = &lv_font_montserrat_20});
controller.open();
page.create(lv_layer_top());                    // fills its parent; a full-screen overlay, a pane, ...
lv_indev_set_group(keypad, page.group());       // keyboard / pad / encoder navigate the page
// every few tens of ms:
page.update();
if (controller.closed()) page.destroy();
```

Keys reach the page as LVGL keys: `LV_KEY_PREV` / `LV_KEY_NEXT` (arrows, Tab, d-pad) move, `LV_KEY_ENTER`
selects and `LV_KEY_ESC` or `LV_KEY_BACKSPACE` go back. `update()` rebuilds the widgets only when what
they show changes (not on every scan result), so a tap or the focus is not lost. The pointer cursor
and the keypad indev are the application's; `BluetoothPageLvgl` never touches them.

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

## View and layout

```cpp
BluetoothPageView view(controller, {.area = {0, 0, 480, 480}, .input_caps = bt_ui_input::Touch});
if (controller.update()) view.draw(display);                 // whole area, any DisplayDriver
controller.activate(view.hit_test(x, y));                    // a tap (ignore -1)
```

The area can be the whole display or a pane. Everything follows from it and from `input_caps`:

- **Text scale:** the title uses 1 / 2 / 3 / 4 for widths under 200 / 360 / 560 / more pixels (smaller if the
  height would leave fewer than about six text lines); everything else is half that size, at least 1.
  The body gets about 19 columns at 128 px wide, 37 at 240 and 60 at 480.
- **Touch and pointer** (`Touch`, `Pointer`) make every row and button at least 28 px high; with only
  `Keyboard`, `Pad` or `Buttons` they keep the compact size and a focus highlight appears (and a
  "Up/Down Enter Esc" hint when nothing else is said). `set_input_caps()` can change this at run time,
  for example when a keyboard connects.
- **Rows** scroll to keep the focused one visible, with a `^` / `v` marker when there are more.
  Below 90 px of height there is no separate status line: its text goes on the right of the header
  when the title leaves room.
- **Row content** grows with the width: marker dot and name; plus the kind tag (KBD, MOUSE, K+M, PAD,
  AUDIO) from 16 columns; plus the state (or signal strength for found devices) from 26.
- **1-bit panels** (`is_monochrome()`): white on black, outlined buttons, the focus is inverted, an
  offline device is a hollow marker.
- Colours come from `BtUiTheme` (a dark default); the text is the 5x8 font of `libs/screen`.

## Input

```cpp
BluetoothPageInput input({.hid = &hid_source, .has_touch = true, .buttons = {0, 1, 2, 3}});
// about 60 times a second:
BtUiPointer touch{pressed, x, y};                       // display pixels (calibrate a resistive panel first)
input.poll(controller, view, has_touch ? &touch : nullptr, buttons.poll());
if (controller.update()) view.draw(display);
```

`hid` is any `HidSource` (`BluetoothHidSource`, `UsbHidSource` or an application merge), so keyboards,
mice and gamepads need nothing more. `poll()` also tells the view which inputs are present
(`capabilities()`), so controls become touch-sized when a mouse connects and get a focus highlight
when a keyboard or pad does.

| Input | Does |
|---|---|
| Keyboard | arrows and Tab (Shift+Tab backwards) move, Enter / Space select, Esc / Backspace go back; holding repeats |
| Gamepad | d-pad or left stick move, A selects, B or Back goes back; any connected pad |
| Mouse | a click on a control (the wheel moves the focus); the cursor is the application's to draw |
| Touch | a tap: press and release on the same control; the press already focuses it, dragging off cancels |
| Buttons | `BtUiButtonMap` maps button indices (e.g. from `DebouncedButtons::poll()`) to previous / next / select / back |

## What the components gained for this

`BluetoothHid` and `BluetoothA2dp` now expose, without any callback:

- `change_counter()`, bumped on every event;
- `found_devices()`, the devices seen by the current scan (HID also with `auto_pair` on);
- HID: `connecting()`, `pairing_passkey()`, `last_pairing_error()`; A2DP: `current_device()`,
  `last_error()`.

For the user to choose what to pair, configure the components with `auto_pair = false` (HID) and
`auto_connect = false` (A2DP).

## Tests

`tests/host_lvgl` builds the LVGL view on a headless LVGL (`-DLVGL_DIR=<lvgl v9 checkout>`) and checks its structure and behaviour: items and focus group, taps, keys, focus following, no rebuild on unchanged scan results, passkey and forget flows, destroy / recreate.

`tests/host/test_bluetooth_ui.cpp` drives the model and controller with fake backends (merge and
de-duplication, navigation and clamped focus, detail and forget with the safe default, external forget,
pairing success and failure, passkey, scan stop on leaving, focus kept across list changes), the layout at
128x64, 240, 320x240, 480 and 480x800 (controls inside the area, no overlap, touch size, hit-testing, scrolling)
the input adaptors (key repeat, edges, tap cancel, capability flags), and the view, rendered into a fake display (nothing drawn outside the area, in a pane too). `BT_UI_DUMP=1` prints
the 128x64 and 240 px frames as text. Run them
with the command in AGENTS.md.
