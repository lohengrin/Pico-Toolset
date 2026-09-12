# Waveshare RP2350-PiZero (base, with or without PSRAM)

## Hardware
Waveshare's RP2350-PiZero board: RP2350, an onboard PIO-driven HDMI/DVI
connector (**no built-in screen** -- see "Notes/gotchas"), a native-SDIO uSD
socket, a PIO-USB host port, and (on the PSRAM-fitted variant) an 8MB QSPI
PSRAM chip on GPIO47.

## Status
Implemented. Every component below has a real-hardware-validated preset for
this board.

## Components & presets
| Component | Preset | Purpose |
|---|---|---|
| `pico_toolset_dvi_hdmi` | `pico_sock_cfg` (`common_dvi_pin_configs.h`, not a `pico_toolset::configs::` preset -- see "Notes/gotchas") | This board's onboard HDMI/DVI output |
| `pico_toolset_psram` | `configs::psram::kWaveshareRp2350PiZero` | Onboard PSRAM chip (CS=GPIO47); `psram_init()` reports `present=false` cleanly if the chip isn't fitted, which is how one firmware image covers "with or without PSRAM" |
| `pico_toolset_sdcard` | `configs::sdcard::kWaveshareRp2350PiZero` | uSD in SPI mode over native SDIO wiring (CS=GPIO43, MOSI=GPIO31, MISO=GPIO40, SCK=GPIO30), PIO1/SM0, `gpio_base=16` |
| `pico_toolset_usb_hid` | `configs::usb_hid::kWaveshareRp2350PiZeroHdmi` | PIO-USB host (D+=GPIO28) on PIO2/core0-polled -- this board's "Hdmi profile", for when DVI already owns core1 and PIO0 |

## Resource map
- PIO0: `dvi_hdmi`'s TMDS serialiser + encoder.
- PIO1/SM0: `sdcard` (`gpio_base=16`, required since MISO=GPIO40 is outside
  PIO's default 0-31 window).
- PIO2: `usb_hid` (the `Hdmi` profile moves it off PIO0 specifically to
  avoid the DVI driver above).
- Core1: dedicated to `dvi_hdmi`'s scanbuf worker; `usb_hid` therefore runs
  polled from core0 via `UsbHidHost::task()` (`run_on_core1=false` in the
  `Hdmi` profile) instead of owning its own core, unlike the LCD combination
  below.
- No watchdog-scratch usage in this combination unless you also link
  `fault_handler` or `reset_buttons` (see `AGENTS.md`'s scratch-register
  table before adding one).

## Build
```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S examples -B examples-build
cmake --build examples-build
```
Produces `examples-build/uf2/waveshare_pizero.uf2` (alongside the other 5
combinations in the same pass). USB HID additionally needs the pico-sdk
`tinyusb` submodule initialized (`git -C $PICO_SDK_PATH submodule update
--init lib/tinyusb`) and Pico-PIO-USB, fetched automatically by
`cmake/pico_pio_usb.cmake` unless you set `PICO_PIO_USB_DIR`. To build only
this board manually:
```sh
cmake -S examples/waveshare_pizero -B build-pizero
cmake --build build-pizero
```

## Example
[`examples/waveshare_pizero/`](../examples/waveshare_pizero/).

## Notes/gotchas
- **This board has no built-in screen.** Its own display path is PIO-driven
  HDMI/DVI (`dvi_hdmi`). The 3.5" ILI9486 LCD + XPT2046 touch panel some
  `configs::ili9486::kWaveshareRp2350PiZero`/`configs::xpt2046::
  kWaveshareRp2350PiZero` doc comments describe is an **external** panel
  wired over this board's GPIO/SPI header, not a bundled screen -- see
  [`waveshare_rp2350_pizero_lcd35a.md`](waveshare_rp2350_pizero_lcd35a.md)
  for that combination, which replaces HDMI with the LCD rather than adding
  it.
- `dvi_hdmi` has no `pico_toolset::configs::` preset of its own (that
  component predates the config-struct-for-everything convention, using
  `dvi_serialiser_cfg` literals from vendored `common_dvi_pin_configs.h`
  instead) -- `pico_sock_cfg` is the one this repo's own
  `dvi_hdmi_example.cpp` documents, in its header comment, as this board's
  onboard TMDS connector.
- PSRAM performance tuning (clock above the 30MHz default) is real but
  board-specific and still being validated at the higher end -- see
  `psram_configs.h`'s doc comment on `kWaveshareRp2350PiZero` before raising
  `max_clock_hz` past what's shown there.
