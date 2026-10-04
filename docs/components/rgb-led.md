# rgb_led

PWM-driven RGB LED with gamma correction. Rewritten from Pimoroni's RGBLED (MIT)
as a plain config-struct component.

| | |
|---|---|
| Target / option | `pico_toolset_rgb_led` / `PICO_TOOLSET_BUILD_RGB_LED` (ON) |
| Links | public `pico_stdlib`; private `hardware_gpio`, `hardware_pwm` |
| Example | `components/rgb_led/example/rgb_led_example.cpp` |
| Presets | `configs::rgb_led::kPimoroniPicoDisplayPack` (R 6, G 7, B 8, active-low) |
| Board | [Pimoroni Pico Display Pack](../boards/pimoroni-pico-display-pack.md) |

## Config -- `RgbLedConfig`

`pin_r`, `pin_g`, `pin_b`, `active_low` (no defaults).

## API -- `RgbLed`

- `void init(cfg)`
- `void set_rgb(uint8_t r, uint8_t g, uint8_t b)` -- 8 bits per channel, gamma 2.8,
  16-bit PWM.

## Notes

- Each pin must be on a different PWM slice, or share one coherently.
- The Pico W's onboard LED is on the radio chip and is *not* driven by this
  component.
