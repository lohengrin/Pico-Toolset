# Pimoroni Presto

> RP2350B board with a built-in 480x480 RGB-parallel LCD, capacitive touch,
> uSD, 8 MB PSRAM, 16 MB flash, 7 WS2812 LEDs, piezo, QW/ST connector and an
> RM2 (CYW43) Wi-Fi/Bluetooth module. **Status: derived.** Every pin comes from
> Pimoroni's Presto firmware and pico-sdk's `pimoroni_presto.h`; the PASS1
> example builds but has not yet been run from this repo on hardware.
> Wi-Fi/Bluetooth, QwSTPad and the Multi-Sensor Stick are planned (PASS2) and
> have no component yet.

## Hardware

| Part | Chip | Notes |
|---|---|---|
| MCU | RP2350B (48 GPIO) | `PICO_RP2350A 0`; PIO windows apply ([guide](../guides/pio-gpio-window.md)) |
| Flash | W25Q128JV, 16 MB | flash divider 2 in the SDK header |
| PSRAM | APS6404L, 8 MB | QMI CS1 on GPIO47 |
| LCD | ST7701S, 480x480, 18-bit RGB bus | init over 9-bit SPI, pixels over a PIO-driven parallel bus |
| Touch | FT6236, two points | I2C1, address **0x48** |
| uSD | microSD socket | SPI mode |
| LEDs | 7 x WS2812 (GRB) | behind the panel as ambient backlight |
| Audio | piezo through a transistor | PWM tone only |
| Wireless | RM2 module (CYW43) | PIO SPI |
| Expansion | QW/ST (Qwiic/STEMMA QT) I2C | |

Sources: [pimoroni/presto](https://github.com/pimoroni/presto) (firmware),
pico-sdk `src/boards/include/boards/pimoroni_presto.h`, and the schematic-derived
table in [`reference/presto-schematic-pinout.md`](../reference/presto-schematic-pinout.md)
(**contradicts the firmware in places; this page follows the firmware**).

## Pin map

### LCD (ST7701S) -- `configs::st7701::kPimoroniPrestoHalfRes` / `kPimoroniPrestoFullRes`

| Function | GPIO | Preset field |
|---|---|---|
| RGB565 data D0..D15 | 1..16 | `pin_d0 = 1` |
| unused low-order lines D16/D17 | 17, 18 | driven low by the driver |
| HSYNC | 19 | `pin_hsync` |
| VSYNC | 20 | `pin_vsync` |
| DE | 21 | `pin_de` |
| DOTCLK | 22 | `pin_dot_clk` |
| Init SPI1 CLK / DAT / CS | 26 / 27 / 28 | `spi`, `pin_spi_clk/dat/cs` (no D/C line: 9-bit frames) |
| LCD reset | 44 | not driven (the driver issues a software reset) -- unverified |
| Backlight (PWM) | 45 | `pin_backlight` |

### Touch (FT6236) -- `configs::ft6236::kPimoroniPrestoHalfRes` / `...FullRes`

| Function | GPIO | Preset field |
|---|---|---|
| SDA / SCL (I2C1) | 30 / 31 | `i2c`, `pin_sda`, `pin_scl` |
| INT (active low) | 32 | `pin_int` |
| Reset | 42 | not driven by Pimoroni's firmware -- unverified |
| Address | -- | `addr = 0x48` |

### uSD -- `configs::sdcard::kPimoroniPresto`

| Function | GPIO | Notes |
|---|---|---|
| SCK | 34 | SPI0 |
| MOSI (CMD) | 35 | SPI0 |
| MISO (DAT0) | 36 | SPI0 |
| CS (DAT3) | 39 | |
| DAT1 / DAT2 | 37 / 38 | unused in SPI mode |

All four are SPI0-capable pins, so the driver uses the hardware SPI (no PIO).

### Everything else

| Function | GPIO | Preset |
|---|---|---|
| PSRAM CS | 47 | `configs::psram::kPimoroniPresto` |
| WS2812 data (7 LEDs) | 33 | `configs::ws2812::kPimoroniPresto` (PIO0 SM3, `gpio_base` 16) |
| Piezo | 43 | `configs::piezo::kPimoroniPresto` |
| QW/ST SDA / SCL (I2C0) | 40 / 41 | no preset yet; `PICO_DEFAULT_I2C` in the SDK header |
| User switch | 46 | plain GPIO, active low |
| CYW43 REG_ON / DATA+IRQ / CS / CLK | 23 / 24 / 25 / 29 | PASS2 |

## Components and presets

| Component | Preset | Notes |
|---|---|---|
| [`st7701`](../components/st7701.md) | `kPimoroniPrestoHalfRes` (240x240, default), `kPimoroniPrestoFullRes` (480x480) | caller supplies the framebuffer; derived |
| [`ft6236`](../components/ft6236.md) | `kPimoroniPrestoHalfRes`, `kPimoroniPrestoFullRes` | coordinates scaled to the chosen resolution; derived |
| [`sdcard`](../components/sdcard.md) | `kPimoroniPresto` | native SPI0, 10 MHz; derived |
| [`psram`](../components/psram.md) | `kPimoroniPresto` | 50 MHz placeholder; derived |
| [`ws2812`](../components/ws2812.md) | `kPimoroniPresto` | derived |
| [`piezo`](../components/piezo.md) | `kPimoroniPresto` | derived |
| [`screen`](../components/screen.md) | -- | `St7701Driver` draws into the scanout buffer |

"Derived" = pins and settings follow Pimoroni's firmware; flip to validated
after the example is run on hardware.

## Resource map

| Resource | Used by | Notes |
|---|---|---|
| PIO1 SM0, SM1 | ST7701 pixel + timing state machines | programs use the whole PIO IRQ0/IRQ1 pair; pins 1-22 fit the default 0-31 window |
| PIO0 SM3 | WS2812 | pin 33 needs `pio_set_gpio_base(pio0, 16)` (done via the preset's `gpio_base`) |
| PIO2 | free | reserved for the CYW43 PIO-SPI in PASS2 (`PICO_CYW43_SPI_PIO_PREFERRED_PIO=2`) |
| DMA | 2 channels (ST7701) | chained line/re-arm pair, plus whatever the SD driver claims |
| Core 1 | ST7701 interrupts in the example | the PIO IRQs are served by the core that calls `St7701::init()` |
| SPI1 | ST7701 init only | idle afterwards |
| SPI0 | uSD | |
| I2C1 | touch | |
| I2C0 | QW/ST | |
| PWM | backlight (GPIO45), piezo (GPIO43) | different slices |
| QMI CS1 | PSRAM | brought up before core1 starts |
| Watchdog scratch | none | |

Memory: a 240x240 RGB565 framebuffer is 115 KB, 480x480 is 450 KB of the
520 KB SRAM. The scanout buffer must be in SRAM (PSRAM cannot feed the DMA at
pixel rate); use PSRAM for assets and application data.

## Clocks and power

The SDK board header keeps the default 150 MHz sysclk (Pimoroni's own firmware
header sets 200 MHz: VCO 1200 MHz, post-dividers 6/1, for the 34 MHz pixel
clock headroom and its heavier rendering). The ST7701 driver derives its PIO
divider from `clk_sys` and caps the pixel clock at 34 MHz, so any sysclk works;
at 150 MHz that gives divider 5 (half-res) / 6 (full-res, pixel SM at half of it).
PSRAM runs from the same clock: the preset's 50 MHz ceiling gives divider 3 at
150 MHz and 4 at 200 MHz. If you raise sysclk, re-check PSRAM timing with the
sweep example ([guide](../guides/clocks-and-power.md)).

## Build

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -S examples -B examples-build
cmake --build examples-build --target presto      # or omit --target for every example
```

Produces `examples-build/uf2/presto.uf2`. For the 480x480 variant configure the
single example directly: `cmake -S examples/presto -B build -DPRESTO_FULL_RES=ON`.
The board is `pimoroni_presto`, shipped with pico-sdk >= 2.3 (nothing is vendored
in `boards/`).

## Example

[`examples/presto/main.cpp`](../../examples/presto/main.cpp): status squares
(PSRAM, SD, touch bus, LCD) over colour bars, a touch crosshair with a tone
following X and LED brightness following Y, the user button, and a QW/ST I2C
bus scan every 5 s on USB serial.

## Known issues and gotchas

- **Pin source conflict:** the schematic-derived pin table disagrees with the
  firmware on the LCD bus (shifted by one GPIO), the SD pins and the CYW43
  `WL_GP` entries. Firmware and the SDK header agree with each other; they are
  used here.
- The ST7701 init sequence includes undocumented registers specific to the
  TL040WVS03CT15 panel; it is carried over verbatim and will not necessarily
  suit another ST7701 panel.
- `FT6236` sits at address 0x48 here (0x38 on most boards); two touches in the
  same row are not resolved (a controller limitation noted by Pimoroni).
- Flash and PSRAM operations must finish before core1 serves the scanout
  interrupts, or the panel glitches; do `psram_init()` and SD access before
  launching core1, as the example does ([AGENTS.md](../../AGENTS.md)).
- Rotation is limited to 0 and 180 degrees: the panel scans in raster order.
- Backlight is PWM on GPIO45 with a perceptual curve; `0` is fully off.
- No 8-bit palette mode yet (Pimoroni's firmware has one).

## Sources

- <https://github.com/pimoroni/presto> (firmware, MicroPython; `drivers/st7701`, `modules/c/presto`, `modules/py_frozen/touch.py`, `examples/`)
- pico-sdk `boards/pimoroni_presto.h`
- [`reference/presto-schematic-pinout.md`](../reference/presto-schematic-pinout.md) (schematic table, with its disagreements listed)
