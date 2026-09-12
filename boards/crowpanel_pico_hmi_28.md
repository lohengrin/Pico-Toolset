# CrowPanel PICO HMI 2.8"

## Hardware
Elecrow's CrowPanel PICO HMI 2.8" -- publicly known to combine a 2.8" HMI
screen, an SD card slot, and UART0/UART1 breakouts on a Pico-form-factor
carrier. Exact display controller (driver chip, interface -- SPI/parallel),
touch capability, and full pinout are **not yet confirmed against this
repo's own hardware** and are deliberately left out below rather than
guessed.

## Status
**Planned -- no driver code or validated presets exist for this board in
this repo yet.** Per this repo's own convention (see `AGENTS.md`'s "only add
a preset validated on real hardware" rule), no `<name>_configs.h` preset or
`boards/` pico-sdk header will be added for this board until it's actually
bench-tested.

## Components & presets
None yet. What's needed once hardware is available to validate against:
- A new display-driver component (no existing driver in this repo is known
  to match the panel's controller -- `ili9486` and `ssd1306` target
  different, already-identified controllers).
- Possibly a new touch-driver component, if the panel has touch and the
  controller isn't XPT2046-compatible.
- `pico_toolset_sdcard`: likely reusable as-is once this board's actual SD
  pin wiring is known -- add a new `configs::sdcard::<PascalCaseName>`
  preset once validated, following the pattern already used for the
  Waveshare/Pico-DV presets (don't create a second `sdcard_configs.h`).
- UART0/UART1: no dedicated toolset component exists for this -- pico-sdk's
  own `pico_stdlib`/`hardware_uart` cover it directly; nothing to add here
  unless a reusable abstraction turns out to be worth it.

## Resource map
Not applicable yet -- no components are wired up for this board.

## Build
Not applicable yet -- no example exists.

## Example
None yet.

## Notes/gotchas
- Do not invent pin numbers, presets, or a `boards/` pico-sdk header for
  this board speculatively -- every other board doc in this directory
  reflects real-hardware-validated presets, and this one should follow the
  same bar once hardware is available.
- When this board is bench-tested: add a real board-definition `.h` here if
  it needs `PICO_BOARD`-level defaults (mirroring
  [`waveshare_rp2350_pizero.h`](waveshare_rp2350_pizero.h)), add named
  presets to the relevant `<name>_configs.h` files, add an
  `examples/crowpanel_pico_hmi_28/` full-combination example following the
  shape of the other `examples/*` directories, add it as a new leg in
  [`../examples/CMakeLists.txt`](../examples/CMakeLists.txt)'s superbuild,
  and flip this doc's Status to Implemented.
