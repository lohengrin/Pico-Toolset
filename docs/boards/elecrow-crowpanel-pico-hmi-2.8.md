# Elecrow CrowPanel PICO HMI 2.8"

> RP2040 all-in-one HMI module: 320x240 ST7789 TFT, XPT2046 resistive touch and
> a uSD slot, all sharing **SPI1** with separate chip selects. Model DIS01028P.
>
> Status: *validated* on real hardware -- display (PiCoMonitor, PicoBoot), touch
> and SD (toolset example, PicoBoot's touch UI).

## Hardware

| Item | Value | Source |
|---|---|---|
| MCU | RP2040, dual Cortex-M0+ up to 133 MHz, 264 KB SRAM, 2 MB flash | Elecrow wiki; flash size from PiCoMonitor/PicoBoot |
| Display | 2.8" TN TFT, 320x240, SPI, ST7789-family | wiki |
| Touch | resistive, XPT2046, shares SPI1 | wiki, validated |
| Storage | uSD ("TF") slot, shares SPI1 | wiki, validated |
| Other | USB, Li-ion battery connector (PH2.0-2P), buzzer, BOOT/RST buttons, I2C/UART/GPIO headers | wiki |
| Power | 5 V / 2 A | wiki |
| Size | 57 x 88.7 x 13.4 mm; active area 43.2 x 57.6 mm | wiki |
| Board definition | stock pico-sdk `pico` (no custom board header) | -- |

Vendor page: <https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html>
(a saved capture is in [`../reference/vendor/`](../reference/README.md)). The
wiki shows the LCD/touch wiring only as images; the numbers below come from the
board schematic and the validated presets.

## Pin map

### SPI1 (shared by display, touch and SD)

| Signal | GPIO | Notes |
|---|---|---|
| SCK | 10 | common |
| MOSI | 11 | common |
| MISO | 12 | touch and SD only (the display is write-only) |
| LCD CS | 9 | `St7789Config::pin_cs` |
| LCD DC | 8 | `pin_dc` |
| LCD RESET | 15 | `pin_reset`; manual reset needed (PiCoMonitor) |
| LCD backlight (PWM) | 18 | `pin_backlight` |
| Touch CS | 16 | `Xpt2046Config::pin_cs` |
| Touch PENIRQ | 17 | `pin_irq` (polled, not relied on) |
| SD CS | 22 | `SdCardConfig::pin_cs` |

### Header ports (from the wiki; no toolset component needed)

| Port | Pins |
|---|---|
| I2C | GP20 (SDA), GP21 (SCL) |
| UART0 | GP0 (TX), GP1 (RX) |
| UART1 | GP4 (TX), GP5 (RX) |
| Free GPIO | GP0-GP7, GP19-GP21, GP26-GP28 |

GPIO15 (LCD reset) is **not** broken out to a header; PicoBoot's test LED uses
exposed header pin GPIO19.

## Components and presets

| Component | Preset | Notes |
|---|---|---|
| `pico_toolset_st7789` | `configs::st7789::kElecrowCrowPanelPicoHmi28` | 320x240, offsets 0/0, `madctl` 0x70, inversion off, tearing-effect off, 62.5 MHz |
| `pico_toolset_xpt2046` | `configs::xpt2046::kElecrowCrowPanelPicoHmi28` | 2 MHz |
| (calibration) | `configs::xpt2046::kElecrowCrowPanelPicoHmi28Calibration` + `xpt2046_to_pixel()` | axes swapped, horizontal = `raw_y` 254..3707, vertical = `raw_x` 278..3769 |
| `pico_toolset_sdcard` | `configs::sdcard::kElecrowCrowPanelPicoHmi28` | native hardware SPI on `spi1` (no PIO), 100 kHz identify / 10 MHz fast |

## Resource map

| Resource | Use |
|---|---|
| SPI1 | display + touch + SD (three CS lines) |
| PIO | none |
| DMA | display pixel push (auto-claimed) |
| SRAM | a 320x240 RGB565 `BufferedDisplay` framebuffer is **150 KB** of 264 KB |
| Core0 stack | PiCoMonitor needs 4 KiB (`PICO_STACK_SIZE=0x1000`); the SDK's 2 KiB default overflowed once flash saving was added |

## Clocks and power

Stock RP2040 clocks are fine: the display runs at 62.5 MHz SPI (off the 125 MHz
`clk_peri`), touch at 2 MHz, SD at up to 10 MHz.

## Build

```sh
cmake -S examples/crowpanel_pico_hmi_28 -B build-crowpanel
cmake --build build-crowpanel
```

## Example

[`examples/crowpanel_pico_hmi_28/`](../../examples/crowpanel_pico_hmi_28/):
`St7789` + `BufferedDisplay` + `Screen`/`TextWidget`, touch printing raw
coordinates, SD listing. It does not use the calibration preset or
`xpt2046_to_pixel()`.

## Known issues and gotchas

- **Three devices, one bus, three clocks.** The display driver and the touch
  driver both change the SPI baud rate, but `pico_fatfs` sets its clock only
  during card initialisation. After any display or touch access, restore the SD
  clock before touching the card: `spi_set_baudrate(spi1,
  pico_fatfs_get_clk_fast_freq())` (PicoBoot does this, including for USB MSC
  reads).
- **Never interleave.** Do not call `Xpt2046Touch::read()` or any `SdCard`
  operation while an `St7789`/`BufferedDisplay` write is in flight;
  `finish_pixels_dma()` drains the RX FIFO and waits for BSY before releasing
  the bus.
- **Gamma/VCOM table.** The ST7789 init tuning table is validated only for
  exactly 320x240 (`st7789.cpp` branches on width/height). A different CrowPanel
  size needs its own bench-confirmed table, not a guess.
- **`madctl` 0x70** is this panel's validated non-rotated orientation; rotations
  need their own bench-confirmed value.
- **Touch debounce.** A resistive panel gives spurious samples at touch-down;
  PiCoMonitor's corner-touch zone (outer third of both axes) is debounced over 3
  polls.
- **Flash layout.** `PICO_FLASH_SIZE_BYTES` must stay the real flash size even if
  you reserve the last sectors for settings, because the SDK asserts every
  erase/program against it (see [flash_store](../components/flash-store.md)).
- **No status LED** on this board.

## Sources

- Elecrow wiki (linked above) and the board schematic/PCB Eagle files it
  provides
- Validated by PiCoMonitor (`src/CrowPanelBoard.cpp`), PicoBoot
  (`targets/picoboot_lvgl_crowpanel`) and the toolset's
  `examples/crowpanel_pico_hmi_28`.
