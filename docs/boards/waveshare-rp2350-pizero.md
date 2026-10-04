# Waveshare RP2350-PiZero

> RP2350B board with an onboard DVI/HDMI connector, uSD socket, PIO-USB port
> and a Raspberry-Pi-compatible 40-pin header. **No built-in screen** -- video is
> either the onboard DVI port or an external SPI LCD on the header
> ([Waveshare 3.5" RPi LCD (A)](waveshare-3.5-rpi-lcd-a.md)).
>
> Status: SD, PSRAM and USB-HID presets are *validated* (PicoDoom, TOM6809,
> PicoBoot). The DVI pin set is *validated* by those same projects; the
> toolset's own `examples/waveshare_pizero` has only been built, not run.

## Hardware

| Item | Value | Source |
|---|---|---|
| MCU | RP2350B (dual Cortex-M33 / dual Hazard3, 48 GPIO) | Waveshare wiki |
| Clock (vendor spec) | "up to 150 MHz"; runs well above that in practice, see [clocks](#clocks-and-power) | wiki, client projects |
| SRAM / flash | 520 KB / 16 MB | wiki, `PICO_FLASH_SIZE_BYTES` in the board header |
| PSRAM | Footprint for an 8 MB QSPI chip, chip select GPIO47; **not populated by default** per the wiki | wiki, board header |
| Video | Onboard DVI connector (works with HDMI displays that accept DVI); PIO-driven via `dvi_hdmi`, **not** HSTX (HSTX is a fixed GPIO12-19 range; this board wires TMDS to GPIO32-39) | wiki, TOM6809 |
| Storage | uSD socket wired for 4-bit SDIO; used here in SD-SPI mode over PIO | wiki schematic |
| USB | USB-C (device) plus a PIO-USB port (host or device, USB 1.1) | wiki |
| Header | 40-pin RPi-compatible, 28 usable GPIO, 2 SPI, 2 I2C, 2 UART, PWM | wiki |
| Extras | Li-ion charge/discharge header, temperature sensor, WS2812 on GPIO2 (board header) | wiki, board header |

Vendor page: <https://www.waveshare.com/wiki/RP2350-PiZero>. A saved capture is
in [`../reference/vendor/`](../reference/README.md).

## Pin map

GPIO numbers, not header positions.

### Video (DVI) -- `pico_sock_cfg` in `components/dvi_hdmi/common_dvi_pin_configs.h`

| Signal | GPIO | Notes |
|---|---|---|
| TMDS D0 (+/-) | 36 / 37 | `pins_tmds[0]` = 36 |
| TMDS D1 (+/-) | 34 / 35 | `pins_tmds[1]` = 34 |
| TMDS D2 (+/-) | 32 / 33 | `pins_tmds[2]` = 32 |
| TMDS CLK (+/-) | 38 / 39 | `pins_clk` = 38; `invert_diffpairs = false` |
| DDC SDA / SCL / CEC | 44 / 45 / 46 | not used by libdvi |

All TMDS pins are >= 32, so the PIO block needs `pio_set_gpio_base(pio, 16)`
before `dvi_init()` -- see [guides/pio-gpio-window.md](../guides/pio-gpio-window.md).

### uSD -- `configs::sdcard::kWaveshareRp2350PiZero`

| SDIO signal | SPI role | GPIO | Preset field |
|---|---|---|---|
| CLK | SCK | 30 | `pin_sck` |
| CMD | MOSI | 31 | `pin_mosi` |
| D0 | MISO | 40 | `pin_miso` |
| D3 / CD | CS | 43 | `pin_cs` |
| D1 | -- | 41 | unused in SPI mode |
| D2 | -- | 42 | unused in SPI mode |

PIO1 / SM0, `gpio_base = 16` (MISO = 40 is outside PIO's default 0-31
window), `PICO_PIO_USE_GPIO_BASE=1`. Card-detect is tied to ground.

### USB-PIO host -- `configs::usb_hid::kWaveshareRp2350PiZero*`

| Signal | GPIO |
|---|---|
| D+ | 28 |
| D- | 29 (always D+ + 1) |

### PSRAM -- `configs::psram::kWaveshareRp2350PiZero`

QSPI chip select GPIO47 (`PICO_PSRAM_CS_PIN` in the board header).

### 40-pin header

| Header pin | Signal | Header pin | Signal |
|---|---|---|---|
| 1 | 3V3 | 2 | 5V |
| 3 | GPIO2 (SDA) | 4 | 5V |
| 5 | GPIO3 (SCL) | 6 | GND |
| 7 | GPIO14 | 8 | GPIO4 (TX) |
| 9 | GND | 10 | GPIO5 (RX) |
| 11 | GPIO17 | 12 | GPIO18 |
| 13 | GPIO27 | 14 | GND |
| 15 | GPIO22 | 16 | GPIO23 |
| 17 | 3V3 | 18 | GPIO24 |
| 19 | GPIO11 (MOSI) | 20 | GND |
| 21 | GPIO12 (MISO) | 22 | GPIO25 |
| 23 | GPIO10 (SCLK) | 24 | GPIO8 (CE0) |
| 25 | GND | 26 | GPIO7 (CE1) |
| 27 | GPIO0 (ID_SDA) | 28 | GPIO1 (ID_SCL) |
| 29 | GPIO15 | 30 | GND |
| 31 | GPIO6 | 32 | GPIO9 |
| 33 | GPIO13 | 34 | GND |
| 35 | GPIO19 | 36 | GPIO16 |
| 37 | GPIO26 | 38 | GPIO20 |
| 39 | GND | 40 | GPIO21 |

The external LCD uses GPIO 7, 8, 10, 11, 12, 17, 24 and 25 of this header
(SPI1 + chip selects + DC/RST/touch IRQ); GPIO28/29 (USB-PIO), 30/31/40/43
(SD) and 32-39 (DVI) are not on the header.

## Components and presets

| Component | Preset | Purpose |
|---|---|---|
| `pico_toolset_dvi_hdmi` | `pico_sock_cfg` (C struct, not in `pico_toolset::configs`) | Onboard video |
| `pico_toolset_psram` | `configs::psram::kWaveshareRp2350PiZero` | 8 MB PSRAM (reports `present=false` when the chip is not fitted) |
| `pico_toolset_sdcard` | `configs::sdcard::kWaveshareRp2350PiZero` | uSD over PIO1 |
| `pico_toolset_usb_hid` | `configs::usb_hid::kWaveshareRp2350PiZeroHdmi` | USB host with DVI active (PIO2, polled from core0) |
| `pico_toolset_usb_hid` | `configs::usb_hid::kWaveshareRp2350PiZeroLcd` | USB host with an LCD instead of DVI (PIO0, own core1) |
| `pico_toolset_usb_hid` | `configs::usb_hid::kWaveshareRp2350PiZeroLcdManualCore1` | Same pins as `...Lcd`, consumer owns core1 (init and `task()` from the same core) |
| `pico_toolset_st7796` / `ili9486` / `xpt2046` | see [LCD (A) doc](waveshare-3.5-rpi-lcd-a.md) | External SPI panel |

## Resource map

| Resource | HDMI build | LCD build |
|---|---|---|
| PIO0 | DVI (TMDS SM0-2) | USB-PIO (`...Lcd`) |
| PIO1 | SD (SM0) | SD (SM0) |
| PIO2 | USB-PIO (`...Hdmi`) | free |
| Core1 | DVI scanline worker | USB host, or the consumer's blit loop (`...LcdManualCore1`) |
| SPI1 | free | LCD + touch |
| Watchdog scratch | none unless `reset_buttons`/`fault_handler` are linked (see README table) | same |

libdvi's per-scanline DMA IRQ cannot share a core with Pico-PIO-USB's SOF-timer
IRQ: with both on one core TOM6809 got no picture at all. That is why the HDMI
build polls USB from core0. PicoBoot validated the `...Hdmi` profile (PIO2,
core0-polled) on its LCD target too.

## Clocks and power

Details and rationale in [guides/clocks-and-power.md](../guides/clocks-and-power.md).
Summary of what the validated projects run:

| Build | `clk_sys` | VREG | Notes |
|---|---|---|---|
| HDMI | exactly **252 MHz** | 1.20 V | 200 MHz gave no signal; libdvi derives the TMDS bit clock from `clk_sys` |
| LCD / USB-host | **264 MHz** | 1.20 V | 252 and 264 are multiples of 12 MHz, required by Pico-PIO-USB |
| No USB, no HDMI | 200 MHz is fine | -- | |

Raise the flash QMI divider to 2 *before* raising `clk_sys`, and call
`psram_init()` *after* the clock change. PSRAM `max_clock_hz` in the preset is a
conservative 30 MHz; PicoDoom and PicoBoot run 126 MHz (HDMI) and 132 MHz (LCD)
with VREG 1.20 V, above the chip's ~100 MHz rating (see
[components/psram.md](../components/psram.md)).

## Build

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S examples/waveshare_pizero -B build-pizero
cmake --build build-pizero
```

USB HID needs the pico-sdk `tinyusb` submodule and Pico-PIO-USB (fetched
automatically, or `-DPICO_PIO_USB_DIR=...`). The board is selected with
`PICO_BOARD=waveshare_rp2350_pizero` and
`PICO_BOARD_HEADER_DIRS=<toolset>/boards`.

## Example

[`examples/waveshare_pizero/`](../../examples/waveshare_pizero/): DVI test
pattern on core1, PSRAM bring-up (present or not), SD listing, USB gamepad
polling on core0. It does not set the 252 MHz clock recipe.

## Known issues and gotchas

- **The board header's UART/I2C/WS2812 defaults look wrong.**
  [`boards/waveshare_rp2350_pizero.h`](../../boards/waveshare_rp2350_pizero.h)
  sets UART0 on GPIO0/1, I2C0 on GPIO6/7 and a WS2812 on GPIO2, but the vendor
  header map puts SDA/SCL on GPIO2/3 and TX/RX on GPIO4/5. The defaults appear
  to come from another board's template (several comments still mention a
  "PRO Micro"). Treat them as *unverified*; none of the validated projects use
  them.
- **No plain LED.** There is no single-GPIO LED; test LEDs must be wired to the
  header (PicoBoot uses GPIO15).
- **PIO window.** SD MISO (GPIO40) and the TMDS pins need `PICO_PIO_USE_GPIO_BASE=1`
  at compile time *and* `pio_set_gpio_base(pio, 16)` at run time; missing the
  second step makes `pio_sm_set_config()` fail silently
  ([guide](../guides/pio-gpio-window.md)).
- **DVI framebuffer in PSRAM.** PSRAM sits behind the XIP cache with no
  cross-core coherency. Keep the DVI framebuffer in SRAM; if core1 must read
  PSRAM, do it through DMA (PicoDoom and TOM6809 both hit this).
- **HDMI digital audio** works (PicoDoom, verified 2026-09-13) via the
  `audio_ring` in `dvi_hdmi`; do not call `dvi_audio_sample_buffer_set()` before
  `dvi_init()`.
- **Audio over a USB sound card is not possible** with PIO-USB: TinyUSB has no
  host UAC driver and `pio_usb_host.c` has no isochronous support.
- **Stack cost of `usb_hid`.** Earlier versions reserved a 16 KB core1 stack even
  with `run_on_core1=false`; the allocation is now lazy. If SRAM is tight, check
  your toolset revision.

## Sources

- Waveshare wiki, <https://www.waveshare.com/wiki/RP2350-PiZero>
- Validated by PicoDoom (`docs/HDMI_PLAN.md`, `src/PicoDoom.cpp`), TOM6809
  (`README-PICO.md`) and PicoBoot (`README.md`, `targets/picoboot_lvgl_dvi`).
