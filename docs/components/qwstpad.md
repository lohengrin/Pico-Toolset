# qwstpad

Pimoroni QwSTPad (ten buttons, four LEDs behind a TCA9555 I2C expander).

| | |
|---|---|
| Target / option | `pico_toolset_qwstpad` / `PICO_TOOLSET_BUILD_QWSTPAD` (ON) |
| Links | public `pico_stdlib`, `hardware_i2c` |
| Example | `components/qwstpad/example/qwstpad_example.cpp` |
| Presets | `kPimoroniPresto` (I2C0, address 0x21; validated) |
| Boards | [Pimoroni Presto](../boards/pimoroni-presto.md) (QW/ST connector) |

## API

- `QwstPadConfig { i2c; addr }` -- the caller brings the bus up (it is shared along a QW/ST chain). Address jumpers give 0x21 / 0x23 / 0x25 / 0x27, so up to four pads share a bus.
- `bool init(cfg)` -- configures the expander; false if the device does not answer. The LEDs show the pad's address position (LED 1 for 0x21, ...).
- `uint16_t read_buttons()` -- bits from `qwstpad_button::` (`A B X Y Up Down Left Right Plus Minus`); 0 on bus error.
- `set_leds(mask)`, `set_led(1..4, on)`, `leds()`, `address_code()`.

Lineage: Pimoroni's qwstpad-micropython (MIT).

## As a gamepad

`qwstpad_gamepad.h`: `qwstpad_gamepad_state(read_buttons())` converts the button bits to the toolset's
`GamepadState` (A/B/X/Y and D-pad one to one, `+` = Start, `-` = Back). `merge(a, b)` in `hid_gamepad.h` combines
it with a USB/Bluetooth pad (buttons OR-ed, sticks from the analogue pad). Used by PicoDoom's Presto input.

