# Pimoroni Pico DV Demo Base

> Carrier board for a Raspberry Pi Pico: HDMI/DVI connector, uSD slot, PCM5100A
> I2S line-out DAC and three user buttons. Works with a Pico (RP2040), Pico W
> or Pico 2 (RP2350); the board is the same, only `PICO_BOARD` differs.
>
> Status: **partial.** SD and I2S presets are *validated*. HDMI is *validated*
> by PicoBoot (Pico W) but the toolset's combined HDMI + SD + I2S example has
> never been run, and it likely has a PIO clash (see
> [Known issues](#known-issues-and-gotchas)).

## Hardware

| Item | Value | Source |
|---|---|---|
| Video | HDMI connector, driven by PicoDVI-style PIO bit-banging (`dvi_hdmi`); needs an overclocked MCU | Pimoroni, PicoDVI |
| Audio | PCM5100A DAC, line-out over I2S | Pimoroni |
| Storage | uSD slot, native SDIO wiring (used as SD-SPI over PIO) | validated |
| Buttons | 3 user buttons A/B/C plus a reset button | Pimoroni, TOM6809 |
| MCU | any Pico variant in the sockets: Pico (RP2040), Pico W (RP2040 + CYW43439), Pico 2 (RP2350) | -- |

The board's pinout does **not** match the RP2350 HSTX peripheral, so HSTX cannot
be used on a Pico 2 here; `dvi_hdmi` uses PIO on all variants. Product page:
<https://wholesale.pimoroni.com/en-us/products/pimoroni-pico-dv-demo-base>. The vendor
pages do not publish GPIO numbers (only a schematic PDF); the numbers below come
from validated presets and the vendored PicoDVI pin table.

## Pin map

### Video (DVI) -- `pimoroni_demo_hdmi_cfg` in `components/dvi_hdmi/common_dvi_pin_configs.h`

| Signal | GPIO | Notes |
|---|---|---|
| TMDS D0 / D1 / D2 (positive pin of each pair) | 8 / 10 / 12 | `pins_tmds = {8, 10, 12}` |
| TMDS CLK | 6 | `pins_clk = 6` |
| Pair polarity | inverted | `invert_diffpairs = true` |

*Validated* by PicoBoot on a Pico W (`targets/picoboot_lvgl_dvi`) at
`clk_sys` 252 MHz, VREG 1.20 V.

### uSD -- `configs::sdcard::kPicoDvCarrier`

| SDIO signal | SPI role | GPIO |
|---|---|---|
| CLK | SCK | 5 |
| CMD | MOSI | 18 |
| DAT0 | MISO | 19 |
| DAT3 | CS | 22 |

The card's four data lines are GPIO19-22. PIO1 / SM0, `gpio_base = -1` (all pins
below 32), `clk_slow_hz` 100 kHz, **`clk_fast_hz` 10 MHz**. The library default
of 50 MHz corrupts the boot-sector read after the slow-clock identification
phase, and the failure reads as "no card".

### I2S audio -- `configs::i2s_audio::kPicoDvCarrier`

| Signal | GPIO | Notes |
|---|---|---|
| DATA | 26 | `pin_data` |
| BCK | 27 | `pin_clock_base` |
| LRCK | 28 | derived: `pin_clock_base + 1` (BCK and LRCK must be consecutive) |

44.1 kHz, mono (duplicated to both channels), DMA channel 0, PIO state machine
0 of the block `pico_audio_i2s` selects (PIO0 by default).

### Buttons

| Button | GPIO | Notes |
|---|---|---|
| A | 14 | active-high: 3V3 when pressed, so pull-**down** inputs |
| B | 15 | |
| C | 16 | |

*Validated* by TOM6809 and PicoBoot (PicoBoot auto-detects polarity at start-up
with `lvgl_gpio_keys` `kAuto`). There is no `reset_buttons` preset for this
board; pass `{14, 15, 16}` and `active_low = false`.

## Components and presets

| Component | Preset | Notes |
|---|---|---|
| `pico_toolset_dvi_hdmi` | `pimoroni_demo_hdmi_cfg` | not in `pico_toolset::configs` |
| `pico_toolset_sdcard` | `configs::sdcard::kPicoDvCarrier` | |
| `pico_toolset_i2s_audio` | `configs::i2s_audio::kPicoDvCarrier` | needs pico-extras (`PICO_EXTRAS_PATH`) |
| `pico_toolset_reset_buttons` | none | pins 14/15/16, active-high |

## Resource map

| Resource | Use |
|---|---|
| PIO0 | `pico_audio_i2s` takes SM0 by default; `dvi_hdmi`'s config also names `pio0` (SM0-2) -- **clash**, see below |
| PIO1 | SD, SM0 |
| DMA 0 | I2S |
| Core1 | DVI scanline worker (HDMI builds) |
| Watchdog scratch | `reset_buttons` tag reboot, if used |

RP2040 has only PIO0 and PIO1. DVI needs three state machines of one block, I2S
one, SD one.

## Clocks and power

- **RP2040 + HDMI:** `clk_sys` 252 MHz with VREG 1.20 V (validated by PicoBoot);
  see [guides/clocks-and-power.md](../guides/clocks-and-power.md).
- **Pico 2:** no project has run HDMI on a Pico 2 on this carrier. If you do,
  apply the same recipe as the PiZero (252 MHz, VREG 1.20 V, flash QMI divider
  raised first) -- *unverified*.
- The 6809-emulation workload in TOM6809 exceeds the 20 ms frame budget on this
  board for some models; that is CPU throughput, not hardware.

## Build

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras
cmake -S examples/pico_dv -B build-pico2 -DPICO_BOARD=pico2 -DEXAMPLE_OUTPUT_NAME=pico_dv_pico2
cmake --build build-pico2
```

Use `-DPICO_BOARD=pico` for a Pico, `pico_w` for a Pico W. The `examples/`
superbuild builds all three.

## Example

[`examples/pico_dv/`](../../examples/pico_dv/): DVI test pattern on core1, SD
listing, 440 Hz I2S tone, three debounced buttons that reboot with a tag.
Build-and-link only; not run on hardware.

## Known issues and gotchas

- **Probable PIO clash in the example.** `pimoroni_demo_hdmi_cfg` sets
  `.pio = pio0` (SM0-2); `pico_audio_i2s` also lands on PIO0 SM0; SD is on PIO1
  SM0. On RP2040 that leaves no clean assignment as written. A workable layout
  (*unverified*): DVI on PIO0, I2S moved to PIO1 SM0 (pico-extras exposes
  `PICO_AUDIO_I2S_PIO`), SD on PIO1 SM1. No project has run HDMI + I2S + SD
  together on this board.
- **RGB332 canvas.** PicoBoot fits 320x240 into RAM by using an RGB332 canvas,
  dithered; whites look slightly yellow.
- **RP2040 DVI vs `flash_safe_execute()`.** The per-scanline DMA IRQ has a lower
  number than the SIO FIFO IRQ and can starve the "park this core" request, so
  `flash_safe_execute()` times out. PicoBoot's fix: `irq_set_priority(
  SIO_FIFO_IRQ_NUM(get_core_num()), 0)` on core1 plus
  `flash_safe_execute_core_init()` in core1's entry. Video stalls briefly while
  flashing.
- **Pico W LED.** On a Pico W the LED hangs off the CYW43 radio chip; to blink it
  link `pico_cyw43_arch_none`. Nothing else in this combination uses wireless.
- **No PSRAM.** PSRAM support targets the PiZero's chip; a Pico 2 on this carrier
  has none to bring up.
- **SD clock.** Keep `clk_fast_hz` at 10 MHz over this board's wiring.

## Sources

- Pimoroni product page and schematic PDF (linked above)
- Wren6991/PicoDVI (`common_dvi_pin_configs.h`, overclock notes)
- Validated by TOM6809 (`README-PICO.md`, `docs/AGENTS/ARCHITECTURE.md`) for
  buttons/SD/I2S and PicoBoot (`targets/picoboot_lvgl_dvi`) for HDMI.
