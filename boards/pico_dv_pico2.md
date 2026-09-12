# "Pico DV" carrier + Pico 2

## Hardware
The same "Pico DV" carrier board as [`pico_dv_pico1.md`](pico_dv_pico1.md)
(onboard PCM5100A-style I2S DAC, native-SDIO uSD socket, HDMI/DVI connector,
3 momentary buttons), fitted with a Pico 2 (RP2350) instead of a Pico 1.

## Status
Partial -- identical caveats to the plain-Pico1 doc (see there for detail):
`sdcard`/`i2s_audio` presets are validated for this carrier, `dvi_hdmi`'s
pin config is an unconfirmed placeholder, `reset_buttons` has no board
preset. `pico_toolset_psram` is *not* part of this combination -- PSRAM
support in this toolset targets the specific PSRAM chip wired on the
Waveshare RP2350-PiZero (`cs_pin`/QSPI wiring is board-specific, see
`psram_configs.h`), not a generic "any RP2350 board" feature; a plain Pico 2
on this carrier has no PSRAM chip to bring up.

## Components & presets
Identical to [`pico_dv_pico1.md`](pico_dv_pico1.md)'s table -- same carrier,
same pins, same presets (none of them are RP2040-vs-RP2350-specific).

## Resource map
Same as [`pico_dv_pico1.md`](pico_dv_pico1.md). RP2350's wider PIO GPIO
addressing (a 32-pin window anywhere in 0-47, vs RP2040's fixed 0-31) isn't
needed here since every pin this combination uses is already below 32.

## Build
```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras   # needed by i2s_audio
cmake -S examples -B examples-build
cmake --build examples-build
```
Produces `examples-build/uf2/pico_dv_pico2.uf2` (built alongside the other
5 combinations in the same pass). To build only this one manually:
```sh
cmake -S examples/pico_dv -B build-pico2 -DPICO_BOARD=pico2 -DEXAMPLE_OUTPUT_NAME=pico_dv_pico2
cmake --build build-pico2
```

## Example
[`examples/pico_dv/`](../examples/pico_dv/) -- shared verbatim with the Pico
1 and Pico 1 W variants; only `-DPICO_BOARD=pico2` differs.

## Notes/gotchas
- Everything in [`pico_dv_pico1.md`](pico_dv_pico1.md)'s "Notes/gotchas"
  applies here too.
- Don't confuse this with the Waveshare RP2350-PiZero combinations
  ([`waveshare_rp2350_pizero.md`](waveshare_rp2350_pizero.md)): both use an
  RP2350, but this is a different carrier board with a different (and
  mostly unvalidated) DVI pin mapping and no PSRAM chip.
