# "Pico DV" carrier + Pico 1 W

## Hardware
The same "Pico DV" carrier board as [`pico_dv_pico1.md`](pico_dv_pico1.md)
(onboard PCM5100A-style I2S DAC, native-SDIO uSD socket, HDMI/DVI connector,
3 momentary buttons), fitted with a Pico W (RP2040 + CYW43439 wireless)
instead of a plain Pico.

## Status
Partial -- identical caveats to the plain-Pico1 doc (see there for detail):
`sdcard`/`i2s_audio` presets are validated for this carrier, `dvi_hdmi`'s
pin config is an unconfirmed placeholder, `reset_buttons` has no board
preset. Nothing in this combination uses the Pico W's wireless chip, so
there's no additional wireless-specific validation gap beyond that.

## Components & presets
Identical to [`pico_dv_pico1.md`](pico_dv_pico1.md)'s table -- same carrier,
same pins, same presets. See that doc.

## Resource map
Same as [`pico_dv_pico1.md`](pico_dv_pico1.md). The Pico W's onboard CYW43439
uses its own dedicated SPI-like bus on fixed pins internal to the module
(not exposed on the carrier's own GPIO map) and is not driven by anything in
this combination, so it adds no additional resource claims here.

## Build
```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras   # needed by i2s_audio
cmake -S examples -B examples-build
cmake --build examples-build
```
Produces `examples-build/uf2/pico_dv_pico1w.uf2` (built alongside the other
5 combinations in the same pass). To build only this one manually:
```sh
cmake -S examples/pico_dv -B build-pico1w -DPICO_BOARD=pico_w -DEXAMPLE_OUTPUT_NAME=pico_dv_pico1w
cmake --build build-pico1w
```

## Example
[`examples/pico_dv/`](../examples/pico_dv/) -- shared verbatim with the Pico
1 and Pico 2 variants; only `-DPICO_BOARD=pico_w` differs.

## Notes/gotchas
- Everything in [`pico_dv_pico1.md`](pico_dv_pico1.md)'s "Notes/gotchas"
  applies here too.
- `-DPICO_BOARD=pico_w` links the SDK's `pico_cyw43_arch` support
  transparently for board-default pins (e.g. the onboard LED, which on a
  Pico W is wired through the wireless chip rather than a plain GPIO) --
  this combination doesn't call any `cyw43_arch_*` API itself, so that's
  invisible unless you add wireless functionality later.
