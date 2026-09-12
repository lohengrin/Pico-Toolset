# Waveshare RP2350-PiZero + Waveshare 3.5" RPi LCD (A)

## Hardware
A Waveshare RP2350-PiZero (see
[`waveshare_rp2350_pizero.md`](waveshare_rp2350_pizero.md) for the bare
board) with an external Waveshare 3.5" RPi LCD (A) panel -- ILI9486 SPI TFT
+ XPT2046 resistive touch, sharing one SPI bus -- wired over the board's
GPIO/SPI header. The board itself has no built-in screen; this combination
uses the LCD **instead of** the board's onboard HDMI/DVI output, not
alongside it.

## Status
Implemented. Every component below has a real-hardware-validated preset for
this exact combination.

## Components & presets
| Component | Preset | Purpose |
|---|---|---|
| `pico_toolset_ili9486` | `configs::ili9486::kWaveshareRp2350PiZero` | External 3.5" ILI9486 SPI LCD (SPI1: SCK=10, MOSI=11, MISO=12, CS=8, DC=24, RST=25), DMA-backed pixel push |
| `pico_toolset_xpt2046` | `configs::xpt2046::kWaveshareRp2350PiZero` | The LCD's XPT2046 resistive touch, sharing SPI1 with the display (CS=7, IRQ=17) |
| `pico_toolset_psram` | `configs::psram::kWaveshareRp2350PiZero` | Onboard PSRAM chip (CS=GPIO47); `present=false` handled cleanly if unfitted, covering "with or without PSRAM" |
| `pico_toolset_sdcard` | `configs::sdcard::kWaveshareRp2350PiZero` | uSD in SPI mode (CS=GPIO43, MOSI=GPIO31, MISO=GPIO40, SCK=GPIO30), PIO1/SM0, `gpio_base=16` |
| `pico_toolset_usb_hid` | `configs::usb_hid::kWaveshareRp2350PiZeroLcd` | PIO-USB host (D+=GPIO28) on PIO0/core1-dedicated -- this board's "Lcd profile", free to use since no DVI is active here |

## Resource map
- SPI1: shared by `ili9486` and `xpt2046` (MISO=GPIO12 shared; the display
  driver's `finish_pixels_dma()`/`pixels_busy()` split -- see
  `ili9486.h` -- lets a consumer hand the bus to touch cleanly after a
  DMA-backed pixel push instead of racing it).
- PIO0: `usb_hid` (the `Lcd` profile, free here since `dvi_hdmi` is not
  used in this combination).
- PIO1/SM0: `sdcard` (`gpio_base=16`, same as the base board).
- Core1: dedicated to the `usb_hid` host stack (`run_on_core1=true` in the
  `Lcd` profile) -- unlike the HDMI combination, core1 isn't already spoken
  for by DVI here. A consumer whose own core1 loop also drives the LCD's
  chunked blit should use `kWaveshareRp2350PiZeroLcdManualCore1` instead
  (see that preset's doc comment in `usb_hid_configs.h`); this repo's
  example uses the simpler fully-automatic `Lcd` profile.
- No watchdog-scratch usage in this combination unless you also link
  `fault_handler` or `reset_buttons`.

## Build
```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S examples -B examples-build
cmake --build examples-build
```
Produces `examples-build/uf2/waveshare_pizero_lcd35a.uf2` (alongside the
other 5 combinations in the same pass). USB HID prerequisites are the same
as the base board's (tinyusb submodule + Pico-PIO-USB). To build only this
combination manually:
```sh
cmake -S examples/waveshare_pizero_lcd35a -B build-lcd35a
cmake --build build-lcd35a
```

## Example
[`examples/waveshare_pizero_lcd35a/`](../examples/waveshare_pizero_lcd35a/).

## Notes/gotchas
- Don't mix this combination's `usb_hid` preset (`...Lcd`) with the base
  board doc's (`...Hdmi`) -- they differ in `pio_num`/`run_on_core1`
  specifically because this combination has no DVI driver claiming core1/
  PIO0.
- The doc comments on `ili9486_configs.h`/`xpt2046_configs.h` previously
  called this panel "bundled" -- it isn't; the PiZero has no built-in
  screen (see the base board doc's "Notes/gotchas"). Both header comments
  and this doc now describe it as an external panel wired over the header.
- `PsramConfig::max_clock_hz` tuning notes and caveats are identical to the
  base board doc -- see there.
