# usb_hid

PIO-USB TinyUSB **host**: keyboard (typed-ASCII queue, key state, LEDs), mouse,
HID gamepads, XInput and DualSense, with double-buffered cross-core state and a
unified `GamepadState`.

| | |
|---|---|
| Target / option | `pico_toolset_usb_hid` / `PICO_TOOLSET_BUILD_USB_HID` (ON) |
| Dependencies | pico-sdk `tinyusb` submodule (`tinyusb_host`) and Pico-PIO-USB (fetched by `cmake/pico_pio_usb.cmake` or `PICO_PIO_USB_DIR`) |
| CMake variables | `PICO_TOOLSET_USB_HID_KEYMAPS` (`us;fr`), `PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP` (`us`, must be in the list) |
| Example | `components/usb_hid/example/usb_hid_example.cpp` |
| Presets | `kWaveshareRp2350PiZeroLcd`, `kWaveshareRp2350PiZeroHdmi`, `kWaveshareRp2350PiZeroLcdManualCore1` |
| Board | [Waveshare RP2350-PiZero](../boards/waveshare-rp2350-pizero.md) (D+ GPIO28, D- GPIO29) |
| Read first | [USB-PIO and TinyUSB pitfalls](../guides/usb-pio-pitfalls.md) |

## Presets

| Preset | `pin_dp` | `pio_num` | `run_on_core1` | Use when |
|---|---|---|---|---|
| `...Lcd` | 28 | 0 | true | no DVI; core1 and PIO0 are free (`core1_stack_size` 4096 words) |
| `...Hdmi` | 28 | 2 | false | DVI owns core1 and PIO0; poll `UsbHidHost::task()` from core0 |
| `...LcdManualCore1` | 28 | 0 | false | you own core1 and interleave `task()` with your own work; call `init()` and `task()` from that same core |

## Config -- `UsbHidConfig`

Required: `pin_dp`, `pio_num`, `run_on_core1`. Optional: `core1_stack_size` 4096
(words), `rhport` 1, `max_hid_interfaces` 4, `max_devices` 4, `enable_keyboard`/
`enable_mouse`/`enable_gamepad`/`enable_xinput` true, `max_gamepads` 4,
`mouse_max_x` 639, `mouse_max_y` 479, `keymap_index` (default keymap),
`numlock_initial_state` true, `led_boot_animation` true.

## API -- `UsbHidHost`

- `bool init(cfg)`, `static void task()` (only when `run_on_core1 = false`).
- Keyboard: `is_key_down(usage)`, `is_modifier_down(mask)`,
  `consume_key_press(usage)`, `uint8_t consume_typed_ascii_char()` (edge-triggered,
  layout from `keymap_index`), `numlock_on()`, `capslock_on()`, `scrolllock_on()`
  (the NumLock state cannot be queried from a keyboard, hence the tracker).
- Mouse: `MouseState mouse_state()` (`present`, `x`, `y`, `left_button`,
  `right_button`, `middle_button`), `consume_mouse_delta(int& dx, int& dy)` for
  mouselook.
- Gamepad: `GamepadState gamepad_state(size_t)`, `connected_gamepad_count()`,
  `connected_keyboard_count()`. `GamepadState`: `present`, `buttons` (bitmask
  `kBtA`...`kBtGuide`, bits 0-14), `lx`, `ly`, `rx`, `ry`, `lt`, `rt` (0-255, centre
  128), `is_xinput`, helpers `down(mask)`, `pressed(mask)`.
- Keymaps (`usb_hid_keymap.h`): `Keymap`, `kKeymaps[]`, `kKeymapCount`,
  `kDefaultKeymapIndex`, `default_keymap()`, `keymap_by_name()`,
  `hid_usage_to_ascii(...)`. Implemented layouts: `us`, `fr`.
- Constants: `kMaxKeyUsages` 256, `kAsciiQueueSize` 32, `kMaxGamepadSlots` 4.

## Notes

- RP2350 still uses `OPT_MCU_RP2040` inside the vendored TinyUSB (no RP2350 MCU
  port); rhport 0 stays a CDC device so `pico_stdio_usb` keeps working beside the
  PIO host on rhport 1.
- The component ships its own `tusb_config.h` and defines the `PICO_STDIO_USB_*`
  and `PICO_ENABLE_USB_RESET_*` macros the `LIB_TINYUSB_HOST` trap switches off.
- The host stack owns a whole core by default; endpoint re-arming after an unplug
  is intentionally not retried.
- `clk_sys` must be a multiple of 12 MHz ([clocks](../guides/clocks-and-power.md)).
- A core1 stack is now allocated only when `run_on_core1` is used.

Lineage: HID report-descriptor parsing from the upstream consumer's parsers;
DualSense parsing and the DMA-channel-claim fix added after hardware testing.
