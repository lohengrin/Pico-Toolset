# Pico-Toolset documentation

Reference documentation for the boards and drivers Pico-Toolset supports.
Start here; the top-level [`README.md`](../README.md) is the quick tour and
[`AGENTS.md`](../AGENTS.md) holds the rules for people (and agents) editing the
repository.

## Where to look

| I want to... | Go to |
|---|---|
| Know which GPIO does what on a given board | [`boards/`](boards/README.md) |
| Use or extend one driver | [`components/`](components/README.md) |
| Understand a cross-cutting hardware constraint (clocks, PIO windows, shared SPI bus, USB-PIO) | [`guides/`](guides/README.md) |
| Read an unmodified vendor wiki capture or datasheet pointer | [`reference/`](reference/README.md) |
| See what the last documentation audit found and what is still open | [`AUDIT.md`](AUDIT.md) |

## Boards

| Board | Chip | Doc |
|---|---|---|
| Waveshare RP2350-PiZero | RP2350B | [`boards/waveshare-rp2350-pizero.md`](boards/waveshare-rp2350-pizero.md) |
| Waveshare 3.5" RPi LCD (A) (and the SunFounder ST7796U look-alike) on the PiZero | -- | [`boards/waveshare-3.5-rpi-lcd-a.md`](boards/waveshare-3.5-rpi-lcd-a.md) |
| Pimoroni Pico DV Demo Base + Pico / Pico W / Pico 2 | RP2040 / RP2350 | [`boards/pimoroni-pico-dv-demo-base.md`](boards/pimoroni-pico-dv-demo-base.md) |
| Elecrow CrowPanel PICO HMI 2.8" | RP2040 | [`boards/elecrow-crowpanel-pico-hmi-2.8.md`](boards/elecrow-crowpanel-pico-hmi-2.8.md) |
| Pimoroni Pico Display Pack | RP2040 | [`boards/pimoroni-pico-display-pack.md`](boards/pimoroni-pico-display-pack.md) |

## Components and libraries

See [`components/README.md`](components/README.md) for the full table and
dependency map. Display: `ssd1306`, `ili9486`, `st7789`, `st7796`, `screen`,
`lvgl_display`. Touch: `xpt2046`. Video: `dvi_hdmi`. USB: `usb_hid`,
`usb_composite`. Storage: `sdcard`, `flash_store`. Memory: `psram`. Audio:
`i2s_audio`. Misc: `reset_buttons`, `rgb_led`, `fault_handler`, and the shared
`driver_interfaces`.

## Conventions used in these documents

- **GPIO numbers are RP2040/RP2350 GPIO numbers**, never Raspberry Pi BCM
  numbers or 40-pin header positions, unless a column says "header pin".
- **Validation status** is one of: *validated* (confirmed on real hardware by a
  named project), *derived* (taken from a schematic or vendor document, not yet
  confirmed), *unverified* (inferred, or documented by the code but never
  exercised). Anything not marked is derived from the code presets.
- Every pin table is checked against the `<component>_configs.h` preset it
  describes; if a document and a preset disagree, the preset is the truth and
  the document is a bug.
- File layout rules: one document per board in `boards/`, one per component in
  `components/`, cross-cutting topics in `guides/`.
