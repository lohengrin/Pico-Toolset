# hid_source

`pico_toolset/hid_source.h` (in `components/hid_input`, header-only, no dependencies): the
transport-neutral view of attached HID input that UI and game code can be written against once.

| | |
|---|---|
| Header | `pico_toolset/hid_source.h` (`HidSource`, `HidMouse`; `GamepadState` from `hid_gamepad.h`) |
| Adapters | `UsbHidSource` (`usb_hid_source.h`), `BluetoothHidSource` (`bluetooth_hid_source.h`) |
| Consumers | `lvgl_hid_init(HidSource&)` ([lvgl-display](lvgl-display.md)); PicoDoom's `I_StartTic()` and boot menu |

`HidSource` has: `is_key_down(usage)`, `is_modifier_down(mask)`, `mouse_state()` (`HidMouse`: present, absolute
x/y, left/right/middle and back/forward), `consume_mouse_delta(dx, dy)`, `consume_mouse_wheel()`,
`gamepad_state(i)` and `connected_keyboard_count()`. Applications can implement it themselves to merge devices
(PicoDoom's Presto build ORs a QwSTPad into a Bluetooth gamepad this way).
