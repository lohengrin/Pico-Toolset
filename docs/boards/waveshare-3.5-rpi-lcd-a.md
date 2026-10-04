# Waveshare 3.5" RPi LCD (A) on the RP2350-PiZero

> 480x320 SPI TFT with resistive touch, plugged onto the PiZero's 40-pin header
> through the Raspberry-Pi-style 26-pin connector. Two panels were validated
> on identical wiring: the genuine Waveshare panel (ILI9486) and a SunFounder
> 3.5" IPS panel (ST7796U). Both sit behind a SPI-to-parallel shift-register
> bridge, so both use the 16-bit-padded command protocol.
>
> Status: *validated* on real hardware (PicoDoom, TOM6809, PicoBoot).

The PiZero has no screen of its own; this panel is an **external add-on that
replaces the HDMI output**, not something bundled with the board. See
[Waveshare RP2350-PiZero](waveshare-rp2350-pizero.md) for everything that is not
the panel.

## Hardware

| Item | Value |
|---|---|
| Resolution | 480 x 320, RGB565 |
| Controller | ILI9486 (Waveshare LCD (A)) or ST7796U (SunFounder 3.5" IPS) |
| Touch | XPT2046 resistive; SPI bus shared with the LCD, separate CS |
| Interface | 26-pin RPi connector. LCD is **write-only**: MISO on the header belongs to the touch controller, so display register reads return zeros (a false negative, not a fault) |
| Draw current | about 150 mA, 0-70 C (vendor FAQ) |

Vendor page: <https://www.waveshare.com/wiki/3.5inch_RPi_LCD_(A)>. A saved
capture is in [`../reference/vendor/`](../reference/README.md).

## Pin map

Validated wiring on the PiZero. The same pin set is used by the ILI9486, ST7796
and XPT2046 presets.

| Signal | LCD connector pin | PiZero GPIO | Preset field |
|---|---|---|---|
| SCK (LCD + touch) | 23 | 10 | `pin_sck` |
| MOSI (LCD + touch) | 19 | 11 | `pin_mosi` |
| MISO (touch only) | 21 (TP_SCL) | 12 | `pin_miso` |
| LCD chip select | 24 | 8 | `pin_cs` |
| LCD data/command (RS) | 18 | 24 | `pin_dc` |
| LCD reset | 22 | 25 | `pin_rst` |
| Touch chip select | 26 | 7 | `Xpt2046Config::pin_cs` |
| Touch PENIRQ | 11 | 17 | `Xpt2046Config::pin_irq` |
| Backlight | -- | none | `pin_backlight = 255` (always on) |

All of these are on **SPI1**: on this board neither the SCK/MOSI/MISO group nor
the SD group has an SPI0 alternate function, and the RP2350 has only one other
SPI, so the LCD gets hardware SPI1 and the SD card runs on PIO.

### Generic wiring on a plain Pico 2 (not validated)

PicoDoom's `docs/# Wave Share LCD Touch.md` describes a different, plain
Pico 2 wiring: SPI0, SCK GP18, MOSI GP19, MISO GP16, LCD CS GP20, DC GP21,
RST GP22, touch CS GP15, IRQ GP13, 20 MHz. It is not PiZero wiring, its sample
init ignores the shift-register protocol (it would produce a white panel), and
it conflicts with the validated pins above. Treat it as non-authoritative.

## Components and presets

| Component | Preset | Notes |
|---|---|---|
| `pico_toolset_ili9486` | `configs::ili9486::kWaveshareRp2350PiZero` | `spi_freq_hz` 8 MHz commands, `pixel_freq_hz` 25 MHz, DMA on |
| `pico_toolset_st7796` | `configs::st7796::kWaveshareRp2350PiZero` | 480x320, MADCTL MV\|BGR (0x28), colour inversion on, `spi_freq_hz` 132 MHz, DMA off |
| `pico_toolset_xpt2046` | `configs::xpt2046::kWaveshareRp2350PiZero` | 2 MHz; take `spi_instance` from `lcd.spi()` |
| `pico_toolset_usb_hid` | `configs::usb_hid::kWaveshareRp2350PiZeroLcd` or `...LcdManualCore1` | see [resource map](#resource-map) |
| `pico_toolset_sdcard`, `pico_toolset_psram` | same as the base board | -- |

Pick the panel driver matching your panel's controller; both implement
`DisplayPanel`, so call sites that take a `DisplayPanel&` do not change.

## Wire protocol

Behind the bridge, the controller sees 8-bit parallel data latched by a shift
register, which dictates:

1. A command is one byte with D/C low.
2. Every register parameter is **sixteen bits** (`0x00` pad + value) with D/C
   high.
3. CS is pulsed around *every* command and parameter -- the CS edge latches the
   shift register.
4. Pixel data (after RAMWR) is the one exception: 16-bit RGB565, big-endian, a
   single continuous CS-low burst with **no** padding.

The register init sequence is vendor-specific; a textbook ILI9486 sequence
leaves the panel blank, and a protocol violation shows up as a uniformly white
panel with no error. The ST7796U panel needs the same padding (confirmed from
the SunFounder Linux driver, fbtft `ilitek,ili9486`, `buswidth=8`,
`regwidth=16`), even though its controller is a different family.

## Clocks and speed

| Setting | Value | Status |
|---|---|---|
| ILI9486 pixel clock | **33 MHz** clean; 40 MHz and above corrupt | validated (PicoDoom, TOM6809) |
| Preset default | 25 MHz | conservative |
| ST7796U SPI clock | **132 MHz** = `clk_peri` 264 MHz / 2, artifact-free | validated (PicoDoom, PicoBoot) |
| ST7796U spec ceiling | ~125 MHz (the preset runs 5.6% above spec) | -- |
| Command clock | 8 MHz | -- |

`spi_set_baudrate()` rounds the baud rate *down*, never up, and `clk_peri`
defaults to the 48 MHz USB PLL (SPI capped at 24 MHz). To reach 33 or 132 MHz
run `clk_sys` at 264 MHz and tie `clk_peri` to it -- see
[guides/clocks-and-power.md](../guides/clocks-and-power.md). The ILI9486 is
bandwidth-bound: 480x300x2 = 288 000 bytes per frame takes about 70 ms at 33 MHz
(about 14 fps ceiling); a 320x200 frame at 25 MHz measured about 9 fps. DMA gave
no throughput gain over `spi_write_blocking` (the bus is the bottleneck) but is
immune to USB IRQ jitter.

## Touch

`Xpt2046Touch` reads Z1/Z2 pressure instead of relying on PENIRQ: on the
SunFounder panel PENIRQ never toggled even though it is correctly wired. The
preset's `pressure_threshold = 300` is an untuned generic value.

Panel-specific calibration (axes are rotated relative to the display;
`xpt2046_to_pixel()` applies it):

| Panel | `swap_axes` | Horizontal raw range | Vertical raw range |
|---|---|---|---|
| Waveshare / ILI9486 (the library default) | true | `raw_y` 280 -> 3908 left to right | `raw_x` 3748 -> 274 top to bottom (inverted) |
| SunFounder / ST7796U | true | **inverted**: swap `raw_h_min`/`raw_h_max` of the default | same as default |

The SunFounder inversion is not yet a named preset: TOM6809 and PicoBoot both
apply it in code with `std::swap(cal.raw_h_min, cal.raw_h_max)`. Defaults are one
panel unit's measured corners -- a starting point, not a constant.

## Resource map

| Resource | Use |
|---|---|
| SPI1 | LCD + touch (shared bus; SD does not use SPI1 here) |
| PIO0 | USB-PIO for the `...Lcd` / `...LcdManualCore1` profiles |
| PIO1 | SD |
| DMA | LCD pixel push (auto-claimed; ILI9486 only; ST7796 at 132 MHz runs synchronous) |
| Core1 | USB host (`...Lcd`) or the consumer's blit loop (`...LcdManualCore1`) |

**USB profile choice.** The preset docs recommend `...Lcd` (PIO0, own core1).
PicoDoom uses `...LcdManualCore1` (core1 interleaves USB polling with chunked
blits). PicoBoot validated `kWaveshareRp2350PiZeroHdmi` (PIO2, core0-polled)
even on the LCD target. All three work; pick by who owns core1.

## Build

```sh
cmake -S examples/waveshare_pizero_lcd35a -B build-lcd35a
cmake --build build-lcd35a
```

## Example

[`examples/waveshare_pizero_lcd35a/`](../../examples/waveshare_pizero_lcd35a/):
ILI9486 + touch + PSRAM + SD + USB gamepad. There is no ST7796 example for this
wiring yet; swap `Ili9486` for `St7796` and the preset name.

## Known issues and gotchas

- **Touch while the display owns the bus.** Do not call `Xpt2046Touch::read()`
  between `set_window()` and `end_write()`. The display driver's
  `finish_pixels_dma()` drains the SPI RX FIFO and waits out BSY before releasing
  the bus; skipping it made the touch controller read back zeros.
- **Baud before CS.** Change the SPI baud *before* asserting CS for a window:
  `spi_set_baudrate()` disables the peripheral and glitches SCK/MOSI, which
  shifted rows by about one pixel on real hardware. The toolset drivers do this.
- **`set_window()` cost.** It is 11 CS-toggled transactions at the 8 MHz command
  clock plus two baud changes; fewer, taller bands beat many thin ones.
- **Colour inversion** is per-panel for the ST7796U (`invert_colors`); this
  panel batch needs it on.
- **Byte order.** LVGL and framebuffers are native-endian; both controllers want
  big-endian on the wire. Skipping the swap gives wrong colours.

## Sources

- Waveshare wiki, <https://www.waveshare.com/wiki/3.5inch_RPi_LCD_(A)>
- SunFounder 3.5" IPS panel and its fbtft driver description (see `st7796.h`)
- Validated by PicoDoom (`src/board_config.hpp`, `docs/PLAN.md`), TOM6809
  (`README-PICO.md`, `include/pico/LcdPanel.hpp`) and PicoBoot
  (`targets/picoboot_lvgl_lcd`).
