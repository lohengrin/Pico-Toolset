# Reference material

Unmodified captures of vendor documentation, kept so the board documents can cite
facts without depending on a web page staying online. **These are third-party
documents**, saved for offline reference; the authoritative, maintained
documentation is in [`../boards/`](../boards/README.md) and
[`../components/`](../components/README.md). Re-check the source page before
relying on a number.

## Vendor captures (`vendor/`)

| File | Source | Saved | Used by |
|---|---|---|---|
| [`waveshare-rp2350-pizero-wiki-2026-09-04.md`](vendor/waveshare-rp2350-pizero-wiki-2026-09-04.md) | <https://www.waveshare.com/wiki/RP2350-PiZero> | 2026-09-04 | [PiZero board doc](../boards/waveshare-rp2350-pizero.md) (40-pin header map, SD schematic) |
| [`waveshare-3.5inch-rpi-lcd-a-wiki-2026-09-04.md`](vendor/waveshare-3.5inch-rpi-lcd-a-wiki-2026-09-04.md) | <https://www.waveshare.com/wiki/3.5inch_RPi_LCD_(A)> | 2026-09-04 | [LCD (A) board doc](../boards/waveshare-3.5-rpi-lcd-a.md) (26-pin pinout, shared-bus note, FAQ) |
| [`elecrow-crowpanel-pico-hmi-2.8-wiki-2026-09-12.md`](vendor/elecrow-crowpanel-pico-hmi-2.8-wiki-2026-09-12.md) | <https://www.elecrow.com/wiki/CrowPanel_Pico_HMI_Display-2.8.html> | 2026-09-12 | [CrowPanel board doc](../boards/elecrow-crowpanel-pico-hmi-2.8.md) (specs, header ports) |

The first two were saved by the PicoDoom and TOM6809 projects (identical copies in
both) and consolidated here.

## Not collected

| Item | Where it lives | Why not here |
|---|---|---|
| ST7796S controller datasheet (`ST7796s.pdf`, 3.5 MB) | `PicoDoom/docs/` | binary, large; get it from the controller vendor |
| XPT2046 datasheet (`xpt2046-datasheet.pdf`, 0.8 MB) | `PicoDoom/docs/` | binary; same |
| `# Wave Share LCD Touch.md` | `PicoDoom/docs/`, `TOM6809/docs/Pico2/` | an unreferenced, apparently LLM-written note describing generic Pico 2 wiring (SPI0) and a sample init that ignores the shift-register protocol; it conflicts with the validated PiZero wiring. Summarised, with that warning, in the [LCD (A) board doc](../boards/waveshare-3.5-rpi-lcd-a.md) |
| Pimoroni Pico DV Demo Base / Display Pack schematics | Pimoroni product pages | vendor publishes a schematic PDF only; GPIO numbers in the board docs come from validated presets |

## Client-project documents that hold related hardware knowledge

Their hardware findings are consolidated into the board docs and
[guides](../guides/README.md); read the originals for the full history.

| Project | Files |
|---|---|
| PicoDoom | `docs/HDMI_PLAN.md` (HDMI + audio bring-up), `docs/PLAN.md` (build log), `src/PicoDoom.cpp` (clock recipe), `src/board_config.hpp` (overclock deltas) |
| TOM6809 | `README-PICO.md` (primary hardware reference: profiles, GPIO table, USB-PIO, PSRAM, DVI), `docs/AGENTS/ARCHITECTURE.md` (Pico DV section) |
| PiCoMonitor | `AGENTS.md`, `README.md` (boards, stack and flash notes) |
| PicoBoot | `README.md`, `docs/architecture.md` (boards, flash layout, RP2350 address translation), `targets/*/main.cpp` |
