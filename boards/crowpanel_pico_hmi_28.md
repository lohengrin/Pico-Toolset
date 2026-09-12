# CrowPanel PICO HMI 2.8"

## Hardware
Elecrow's CrowPanel PICO HMI 2.8" -- a Pico-form-factor RP2040 carrier with a
320x240 ST7789-family SPI TFT, an XPT2046 resistive touch controller, and a
uSD ("TF") card slot, all three sharing SPI1 with separate CS lines. Also
breaks out UART0/UART1 and I2C headers (no toolset component needed for
those -- pico-sdk's own `hardware_uart`/`hardware_i2c` cover them directly).

## Status
**Implemented.** Display, touch, and SD card all have real-hardware-derived
presets. The display path was ported from a consumer's already-flying driver;
touch and SD are new capabilities for this board -- bench-confirm both on
first flash (see Notes/gotchas).

## Components & presets
| Component | Preset | Purpose |
|---|---|---|
| `pico_toolset_st7789` | `configs::st7789::kElecrowCrowPanelPicoHmi28` | 320x240 SPI TFT (SPI1: SCK=10, MOSI=11, MISO=12, CS=9, DC=8, RESET=15, backlight PWM=18), DMA-backed pixel push |
| `pico_toolset_xpt2046` | `configs::xpt2046::kElecrowCrowPanelPicoHmi28` | Built-in resistive touch, sharing SPI1 with the display (CS=16, PENIRQ=17) |
| `pico_toolset_sdcard` | `configs::sdcard::kElecrowCrowPanelPicoHmi28` | uSD in native hardware-SPI mode (CS=22), sharing SPI1's SCK/MOSI/MISO with the display and touch |

## Resource map
- SPI1: shared by all three components (`st7789`, `xpt2046`, `sdcard`) --
  MOSI=GPIO11/SCK=GPIO10/MISO=GPIO12 in common, each device selected by its
  own CS (LCD=GPIO9, touch=GPIO16, SD=GPIO22). `St7789Driver::flush()`'s
  `finish_pixels_dma()` drains the RX FIFO and waits out BSY before handing
  the bus back, same contract as `Ili9486`+`xpt2046` document -- don't call
  `Xpt2046Touch::read()` or any `SdCard` operation while an
  `St7789`/`St7789Driver` write is still in flight.
- No PIO/DMA/core/watchdog-scratch usage beyond the display's own DMA
  channel (auto-claimed) -- SD card here uses `sdcard`'s native-hardware-SPI
  path (`SdCardConfig::spi_instance = spi1`), not PIO-bit-banged SPI, so it
  claims no PIO block.
- Framebuffer: a 320x240 RGB565 `St7789Driver` framebuffer is 150KB -- most
  of the RP2040's 264KB SRAM. Watch your total static+heap+stack budget if
  you add much beyond the Screen/Widget composition this board's example
  uses.

## Build
```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S examples -B examples-build
cmake --build examples-build
```
Produces `examples-build/uf2/crowpanel_pico_hmi_28.uf2` (alongside the other
combinations in the same pass). To build only this combination manually:
```sh
cmake -S examples/crowpanel_pico_hmi_28 -B build-crowpanel
cmake --build build-crowpanel
```

## Example
[`examples/crowpanel_pico_hmi_28/`](../examples/crowpanel_pico_hmi_28/).

## Notes/gotchas
- Touch and SD pin numbers came from the board's schematic, not yet from a
  toolset consumer's bench-tested firmware -- confirm both work on real
  hardware before relying on them (the display path, by contrast, is a
  direct port of an already-working consumer driver and should just work).
- The ST7789 init sequence's gamma/VCOM tuning table is currently only
  validated for this exact 320x240 panel (`St7789Config::width == 320 &&
  height == 240` in `components/st7789/src/st7789.cpp`). A different
  CrowPanel panel size would need its own validated table added there, not
  a guessed one.
- `madctl = 0x70` (COL_ORDER | SWAP_XY | SCAN_ORDER) is this panel's
  validated non-rotated orientation; a rotated mount needs a different,
  bench-confirmed MADCTL value, not a computed rotation transform (this
  driver doesn't attempt one -- see `St7789Config::madctl`'s doc comment).
