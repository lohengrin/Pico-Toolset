# Board documentation

One document per physical board (or board + add-on combination). Each follows
the template at the bottom of this page so an agent or a human can find the same
facts in the same place.

| Document | Hardware | Presets used | Example (built by `examples/`) | Status |
|---|---|---|---|---|
| [Waveshare RP2350-PiZero](waveshare-rp2350-pizero.md) | RP2350B, HDMI/DVI, uSD, PIO-USB, optional PSRAM | `psram`, `sdcard`, `usb_hid` (+ DVI pin config) | `waveshare_pizero` | Display path not re-run from this repo; everything else validated |
| [Waveshare 3.5" RPi LCD (A) on the PiZero](waveshare-3.5-rpi-lcd-a.md) | ILI9486 (or ST7796U) + XPT2046 over the 40-pin header | `ili9486` / `st7796`, `xpt2046`, `usb_hid` | `waveshare_pizero_lcd35a` | Validated (ILI9486 and ST7796U) |
| [Pimoroni Pico DV Demo Base](pimoroni-pico-dv-demo-base.md) | HDMI/DVI, uSD, PCM5100A I2S, 3 buttons; Pico, Pico W or Pico 2 | `sdcard`, `i2s_audio` (+ DVI pin config) | `pico_dv_pico1`, `pico_dv_pico1w`, `pico_dv_pico2` | Partial: HDMI validated in PicoBoot, combined example not run |
| [Elecrow CrowPanel PICO HMI 2.8"](elecrow-crowpanel-pico-hmi-2.8.md) | RP2040, ST7789 320x240, XPT2046, uSD | `st7789`, `xpt2046`, `sdcard` | `crowpanel_pico_hmi_28` | Validated |
| [Pimoroni Pico Display Pack](pimoroni-pico-display-pack.md) | ST7789 240x135, RGB LED, 4 buttons on a Pico | `st7789`, `reset_buttons`, `rgb_led` | none (see PiCoMonitor) | Validated in PiCoMonitor |

The pico-sdk board header for the PiZero lives next to this tree, in
[`/boards/waveshare_rp2350_pizero.h`](../../boards/waveshare_rp2350_pizero.h);
the other boards use stock pico-sdk board definitions (`pico`, `pico_w`,
`pico2`).

## Building every example

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras   # needed by the pico_dv legs
cmake -S examples -B examples-build
cmake --build examples-build
```

produces one `.uf2` per combination under `examples-build/uf2/`. See
[`examples/CMakeLists.txt`](../../examples/CMakeLists.txt). Each example forces
its own `PICO_BOARD`/`PICO_PLATFORM`, so a `PICO_PLATFORM` left exported in
your shell by an old `env.sh` cannot make a leg fail with a platform mismatch.
The examples are build-and-link smoke tests: they do not set the clock recipes
the boards need at run time (see
[guides/clocks-and-power.md](../guides/clocks-and-power.md)).

## Template for a new board document

Copy this shape; keep section names so documents stay comparable.

```markdown
# <Board name>

> One-line summary and the validation status.

## Hardware
What is on the board, vendor links.

## Pin map
One table per bus/peripheral: function, GPIO, preset field, notes.

## Components and presets
| Component | Preset | Notes |

## Resource map
PIO blocks, DMA channels, cores, clock requirements, watchdog scratch.

## Clocks and power
Required clk_sys / VREG / clk_peri / flash divider settings.

## Build
Exact command(s) and prerequisites.

## Example
Link to examples/<name>/.

## Known issues and gotchas
Things learned on real hardware.

## Sources
Vendor pages, datasheets, client projects that validated the presets.
```

Only document a pin as validated if a named project ran it on real hardware.
Everything else is "derived" (schematic/vendor document) or "unverified".
