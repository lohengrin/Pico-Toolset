# Board & combination docs

Each file here describes one **assembled board or board+peripheral
combination** end-to-end -- which `pico_toolset_*` components to enable,
which named config preset to call on each, how the pieces avoid resource
conflicts (PIO/DMA/core/watchdog-scratch), and how to build a working
firmware image for it. This is the layer above the per-driver
`<name>_configs.h` presets (see [`AGENTS.md`](../AGENTS.md)): those say "here
is a validated config for this component on this board," these say "here is
the whole board, and which presets its firmware needs."

Pico-sdk `PICO_BOARD` header files (like
[`waveshare_rp2350_pizero.h`](waveshare_rp2350_pizero.h)) also live in this
directory when a board needs one -- a `.md` doc and a `.h` header for the
same board are unrelated file types serving different consumers (CMake vs.
a human/agent) and commonly coexist here.

## Index

| Doc | Hardware | Status |
|---|---|---|
| [`pico_dv_pico1.md`](pico_dv_pico1.md) | "Pico DV" carrier (sdcard, HDMI, I2S audio, 3 buttons) + Pico 1 | Partial |
| [`pico_dv_pico1w.md`](pico_dv_pico1w.md) | Same carrier + Pico 1 W | Partial |
| [`pico_dv_pico2.md`](pico_dv_pico2.md) | Same carrier + Pico 2 | Partial |
| [`waveshare_rp2350_pizero.md`](waveshare_rp2350_pizero.md) | Waveshare RP2350-PiZero (HDMI, PSRAM, sdcard, USB HID) | Implemented |
| [`waveshare_rp2350_pizero_lcd35a.md`](waveshare_rp2350_pizero_lcd35a.md) | Waveshare RP2350-PiZero + external Waveshare 3.5" RPi LCD (A) | Implemented |
| [`crowpanel_pico_hmi_28.md`](crowpanel_pico_hmi_28.md) | CrowPanel PICO HMI 2.8" | Implemented |

## Template for a new board doc

Copy this shape when documenting a new board or combination:

```markdown
# <Board / combination name>

## Hardware
One line: what the physical board/combo is.

## Status
Implemented | Partial | Planned -- and why, if not Implemented.

## Components & presets
| Component | Preset | Purpose |
|---|---|---|
| `pico_toolset_<x>` | `pico_toolset::configs::<x>::<Preset>` | what it drives |

## Resource map
PIO blocks / DMA channels / cores / watchdog-scratch indices this
combination claims, and any conflict already resolved (link the preset doc
comment that explains it rather than re-deriving it here).

## Build
Exact command(s) to produce a working firmware image, plus any prerequisite
(Pico-PIO-USB, pico_fatfs, pico-extras/PICO_EXTRAS_PATH, submodules).

## Example
Link to `examples/<name>/`, or "none yet" with why.

## Notes/gotchas
Board-specific caveats worth knowing before touching this combination.
```

Only add a doc for a combination once you know what's actually true about
it -- for a component preset that means real-hardware validation (see
`AGENTS.md`'s "only add a preset validated on real hardware" rule); for a
board-level doc with no drivers yet (like CrowPanel below), that means
saying so plainly rather than inventing pins or presets.
