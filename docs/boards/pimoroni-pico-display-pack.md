# Pimoroni Pico Display Pack

> Small ST7789 240x135 display with an RGB LED and four buttons that plugs onto
> a Raspberry Pi Pico. Used here through PiCoMonitor.
>
> Status: *validated* by PiCoMonitor (display, buttons, LED). The toolset has no
> example for this board.

## Hardware

| Item | Value |
|---|---|
| MCU | any Pico in the socket (PiCoMonitor builds for Pico and Pico W) |
| Display | 1.14" ST7789, 240 x 135, SPI |
| Buttons | four, A/B/X/Y at the corners |
| LED | one RGB LED |
| Board definition | stock pico-sdk `pico` / `pico_w` |

Search "Pimoroni Pico Display Pack" for the product page and schematic.

## Pin map

| Function | GPIO | Preset field |
|---|---|---|
| SPI0 SCK | 18 | `St7789Config::pin_sck` |
| SPI0 MOSI | 19 | `pin_mosi` |
| LCD CS | 17 | `pin_cs` |
| LCD DC | 16 | `pin_dc` |
| LCD backlight (PWM) | 20 | `pin_backlight` |
| LCD reset | none | `pin_reset = 255`: software SWRESET only |
| Button A (top-left) | 12 | `ButtonsConfig::pins[0]` |
| Button B (bottom-left) | 13 | `pins[1]` |
| Button X (top-right) | 14 | `pins[2]` |
| Button Y (bottom-right) | 15 | `pins[3]` |
| LED red / green / blue | 6 / 7 / 8 | `RgbLedConfig::pin_r/g/b` |

Buttons are **active-low** (pulled up, grounded when pressed); the LED is
**active-low** (common-anode).

## Components and presets

| Component | Preset | Notes |
|---|---|---|
| `pico_toolset_st7789` | `configs::st7789::kPimoroniPicoDisplayPack` | 240x135, `col_offset` 40, `row_offset` 53, `madctl` 0x70, tearing-effect **on**, inversion **on**, 62.5 MHz |
| `pico_toolset_reset_buttons` | `configs::buttons::kPimoroniPicoDisplayPack` | 4 pins, active-low, 3-frame debounce (use `DebouncedButtons::init(const ButtonsConfig&)`) |
| `pico_toolset_rgb_led` | `configs::rgb_led::kPimoroniPicoDisplayPack` | 8-bit per channel, gamma 2.8, 16-bit PWM |

`col_offset`/`row_offset` account for the panel RAM being larger than the
visible area. The tearing-effect/inversion settings match the validated
"PIMORONI" register variant, as opposed to the CrowPanel's.

## Resource map

| Resource | Use |
|---|---|
| SPI0 | display |
| PWM | backlight (GPIO20) and the three LED pins (each on its own slice or sharing coherently) |
| SRAM | a 240x135 RGB565 framebuffer is about 63 KB |
| PIO / DMA | DMA for pixel push (auto-claimed) |

## Clocks and power

Stock clocks.

## Build

There is no example in this repository. PiCoMonitor builds it:
`cmake -S . -B build -DWITH_PICODISPLAY=ON -DWITH_CROWPANEL=OFF` (needs a
Pimoroni `pimoroni-pico` checkout only if you use Pimoroni's own drivers; the
display itself goes through `pico_toolset_st7789`).

## Example

None in the toolset. See PiCoMonitor's `src/PicoDisplayBoard.cpp`.

## Known issues and gotchas

- **Four buttons, not two.** Some project documents say the pack has two
  buttons; the preset and PiCoMonitor use all four (A/B/X/Y on GP12-15).
- The Pico W's LED is on the radio chip, not a GPIO; PiCoMonitor links
  `pico_cyw43_arch_none` only to reach it.
- `St7789::fill_solid` stages a 320-pixel row; panels wider than 320 would
  overflow it (not a concern here).

## Sources

- Pimoroni product page; validated by PiCoMonitor (`src/PicoDisplayBoard.*`).
