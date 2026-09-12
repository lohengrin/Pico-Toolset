# AGENTS.md

Guidance for AI coding agents working in this repository.

## Project overview

Reusable, config-centric device drivers for Raspberry Pi Pico (RP2040/RP2350):
four independent CMake components under `components/` plus the header-only
Screen library under `libs/screen/`. Built with C++20, namespace
`pico_toolset`. Every driver takes a config struct (pins, bus instance,
frequencies, feature toggles) instead of hard-coded wiring, and each
component ships an `example/` program that doubles as a smoke test.

## Component invariants (keep when editing)

- **Config struct for everything**: never hard-code pins/bus/frequencies in
  driver logic. But config structs must NOT default-construct into something
  that happens to work, either -- pins/bus instances/PIO-DMA assignments are
  board wiring, and a member initializer baking in one specific board's
  values is exactly the hardcoding this rule exists to prevent (it just
  hides inside the struct instead of the driver). Leave those fields with no
  default (so `Config{}` value-initializes them to an obviously-invalid 0/
  nullptr, not a silently-plausible wrong board); give a generic *behavioral*
  option (`use_dma`, `run_self_test`, `buffer_count`, ...) a real default if
  one makes sense regardless of board. Ship known-good combinations instead
  in a sibling `<name>_configs.h`, one `inline constexpr` (or `inline const`
  if the type holds a non-constexpr-safe pointer like `spi_inst_t*`/`PIO` --
  the SDK's `spi1`/`pio1` etc. macros are reinterpret_casts, not core
  constant expressions) per validated board+device combination, namespaced
  `pico_toolset::configs::<component>::<PascalCaseBoardName>`. Only add a
  preset for a combination actually validated on real hardware by a
  consumer -- don't invent plausible-looking pin numbers.
- **RP2040 + RP2350**: guard RP2350-only features (PSRAM) with `#if PICO_RP2350`
  and reflect it in CMake (`if(PICO_RP2350)`) so RP2040 builds stay valid.
- **Namespace**: all public API lives in `pico_toolset` (except `extern "C"`
  allocator functions for the PSRAM component).
- **CMake target naming**: `pico_toolset_<name>`; examples are the
  name + `_example`. Top-level `PICO_TOOLSET_BUILD_<NAME>` options gate each
  subdirectory. Components live under `components/`, the header-only Screen
  library under `libs/`.
- **Cross-core safety**: the USB HID host writes from its own core; the app
  reads from another. State visible across cores must be double-buffered or
  otherwise atomically swapped (`volatile` swap pointers -- do not introduce
  mutexes/locks on the host core's time-critical path).
- **Watchdog scratch registers are a shared resource**: `watchdog_hw->scratch[0..7]` is one flat, repo-wide namespace (8 words), not per-component storage -- reset_buttons and fault_handler both use it (to survive a `watchdog_reboot()`) and a consumer can link both. Before claiming a scratch index for a new use, check the allocation table in the top-level README (also mirrored in `libs/fault_handler/include/pico_toolset/fault_handler.h`) and extend it -- never reuse an index another component already owns, and remember `scratch[4]` is reserved by the SDK's own watchdog bookkeeping.
- **No dependencies beyond pico-sdk** unless declared: SSD1306/ILI9486/
  XPT2046/PSRAM/reset_buttons/Screen use pico-sdk only. SD card uses
  elehobica/pico_fatfs (fetched via `cmake/pico_fatfs.cmake` or
  `PICO_FATFS_DIR`, same shape as Pico-PIO-USB below). USB HID uses Pico-PIO-USB (fetched via
  `cmake/pico_pio_usb.cmake` or `PICO_PIO_USB_DIR`); Pimoroni backend is
  optional and only compiled under `PICO_TOOLSET_SCREEN_PIMORONI`. I2S audio
  uses pico-extras' `pico_audio_i2s`, which the *consumer* must import (its
  own `pico_extras_import.cmake` runs before `project()`, same constraint as
  pico-sdk's) -- this component only asserts the target already exists
  (`PICO_TOOLSET_BUILD_I2S_AUDIO` defaults OFF, unlike every other
  component, for exactly this reason).

## Build/verify workflow

Prereqs: `PICO_SDK_PATH` set; RP2350 builds need `-DPICO_BOARD=pico2`.

```sh
cmake -B build -DPICO_BOARD=pico2      # or pico
cmake --build build
```

USB HID additionally needs Pico-PIO-USB (see README). There is no CI or unit
test runner wired up yet; verification is "configure + build cleanly" plus
manual smoke tests of the examples on hardware.

## Conventions

- No code comments unless they explain *why* (e.g. the ILI9486 baudrate
  ordering, the PSRAM RXDELAY clamp, the XInput re-arm guard). Keep them.
- Follow the existing style: 4-space indent, `snake_case` members, trailing
  underscore for private members (`m_` prefix actually used here), `kXxx`
  for constants, `Ssd1306`/`Ili9486` style class names.
- Include guards are `#pragma once`.
- When adding a component: add top-level option + `add_subdirectory`, define
  `pico_toolset_<name>` target + `_example`, update README's component
  table and build-options table, and (once you have a real-hardware-
  validated board+device combination) add its own `<name>_configs.h` -- see
  "Config struct for everything" above.
- **Extend `<name>_configs.h`, don't work around it**: when a consumer
  validates the existing driver on a *new* board, or a new wiring variant of
  a board already covered (e.g. a different profile that moves a pin to
  dodge a peripheral conflict, the way usb_hid's Lcd/Hdmi presets differ
  only in `pio_num`/`run_on_core1`), add a new named preset to that
  component's existing `<name>_configs.h` -- don't create a second configs
  file, don't put board-specific values back as struct defaults, and don't
  let the consuming project hardcode the pins locally instead (if you find
  yourself doing that, the preset belongs here instead, so the next
  consumer on the same board doesn't have to re-derive it). Name it
  `pico_toolset::configs::<component>::<PascalCaseBoardOrVariantName>`,
  document which project/hardware validated it (a link/name is enough), and
  copy the doc-comment style already in that file (wire rationale, any
  gotcha like a required `gpio_base`/DMA-channel/PIO choice). If the new
  preset is for a variant of a board already in the file (like the usb_hid
  Lcd/Hdmi split), keep both presets side by side with a comment on each
  explaining when to pick which -- don't replace the existing one.

## Source lineage (when porting code in)

- SSD1306: David Schramm's rpi-pico-ssd1306 (MIT).
- ILI9486: TOM6809 `Ili9486Display` + PicoDoom DMA (MIT).
- XPT2046: TOM6809 `Xpt2046Touch`/`TouchCalibration`, real-hardware-validated
  on the Waveshare 3.5in RPi LCD (A).
- PSRAM: TOM6809 `Psram`/`PsramMemoryResource`.
- I2S audio: TOM6809 `PicoI2sAudioOutput` (PCM5100A DAC, pico-extras'
  `pico_audio_i2s`).
- SD card: TOM6809 `PicoSdCard` (unifies its two board-specific variants,
  `PicoSdCard_PicoDv`/`PicoSdCard_Waveshare`, into one config-driven driver).
- Reset buttons: TOM6809 `PicoResetButtons` (decomposed into the generic
  `DebouncedButtons` + `watchdog_reboot_with_tag()`/
  `consume_pending_watchdog_tag()` pair -- the Thomson-model-tag mapping
  stays in TOM6809).
- USB HID: TOM6809 `PicoUsbHidInput` (+ pico-infonesPlus descriptor parsers).
  Mouse `middle_button`, the raw-delta `consume_mouse_delta()` (for
  mouselook/aim, alongside the existing clamped-cursor `mouse_state()`), and
  the `kWaveshareRp2350PiZeroLcdManualCore1` config preset (a consumer
  launching and owning core1 itself) ported from PicoDoom's
  `PicoUsbMouse`/`PicoUsbKeyboard`, whose core1 loop interleaves USB polling
  with its own ILI9486 chunked-blit stepping.
- DVI/HDMI: vendored from Waveshare's RP2350-PiZero C example repo (itself
  derived from Wren6991/PicoDVI, BSD-3-Clause), extracted into TOM6809 first
  then moved here once its HDMI/audio work stabilized. HDMI digital audio
  (`audio_ring.*`/`data_packet.*`, new files not vendored) ports rh1tech/
  frank-hdmi-audio's ring design (BSD-3-Clause) and Shuichi Takano's
  `pico_lib` dvi::DataPacket encode algorithm (MIT) -- see
  `components/dvi_hdmi/README.md` for full detail and links, and the
  top-level README's "Credits and third-party code" section for the
  user-facing summary.
  DualSense parsing and the DMA-channel-claim fix in `host_stack_setup()`
  ported from TOM6809's own copy after real-hardware testing found the
  toolset's version missing both.
- Fault handler: PicoDoom `FaultHandler.cpp` -- a Cortex-M33 hard-fault
  handler that stashes PC/LR/CFSR in watchdog scratch registers and reboots,
  reporting on the next boot instead of the SDK's silent default. Moved off
  its original scratch[0..3] onto scratch[2]/[3]/[5]/[6] to not collide with
  reset_buttons' scratch[0]/[1] -- see the scratch-register invariant above.
- Screen: PiCoMonitor.new `Screen`/`Widget` (Pimoroni PicoGraphics default).
- `boards/waveshare_rp2350_pizero.h`: Raspberry Pi (Trading) Ltd.'s pico-sdk
  board header for this board (BSD-3-Clause), as vendored/extended
  identically by both TOM6809 and PicoDoom -- moved here once instead of
  staying duplicated per-project.

Credit upstream in headers when adapting code.