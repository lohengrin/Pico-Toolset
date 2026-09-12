# "Pico DV" carrier + Pico 1

## Hardware
A "Pico DV" carrier board (onboard PCM5100A-style I2S DAC, native-SDIO uSD
socket, HDMI/DVI connector, 3 momentary buttons) fitted with a plain
Raspberry Pi Pico (RP2040).

## Status
Partial. `sdcard` and `i2s_audio` have real-hardware-validated presets for
this carrier. `dvi_hdmi`'s exact TMDS pin wiring for this specific carrier
is **not** independently validated in this repo (see "Notes/gotchas"), and
`reset_buttons` has no board preset file at all (button pins are
per-consumer, not board wiring worth presetting) -- both need adapting/
confirming for your actual board before trusting the example as-is.

## Components & presets
| Component | Preset | Purpose |
|---|---|---|
| `pico_toolset_sdcard` | `configs::sdcard::kPicoDvCarrier` | uSD in SPI mode over native SDIO wiring (CS=GPIO22, MOSI=GPIO18, MISO=GPIO19, SCK=GPIO5), PIO1/SM0 |
| `pico_toolset_i2s_audio` | `configs::i2s_audio::kPicoDvCarrier` | Onboard PCM5100A-style DAC (DATA=GPIO26, BCK=GPIO27, LRCK=GPIO28), DMA0/PIO SM0 |
| `pico_toolset_dvi_hdmi` | none (no validated preset) | PIO-driven DVI/TMDS video-out; see "Notes/gotchas" for the example's placeholder pin config |
| `pico_toolset_reset_buttons` | none (no board preset file) | 3 debounced buttons; pins are consumer-supplied, see the example |

## Resource map
- PIO0: reserved for `i2s_audio` (SM0).
- PIO1: `sdcard` (SM0) -- chosen specifically to avoid PIO0, per
  `sdcard_configs.h`'s own doc comment.
- `dvi_hdmi` needs its own PIO block for the TMDS serialiser (PIO2 on an
  RP2350, or a 2-SM slice of whichever PIO you route it to on RP2040) --
  confirm it doesn't collide with the above once you pick a validated pin
  set for your carrier.
- DMA channel 0 is claimed by `i2s_audio`; `dvi_hdmi` and `sdcard`'s DMA
  usage is internal/auto-claimed (see their own headers) -- don't hardcode
  channel 0 for anything else on this board.
- No watchdog-scratch usage in this combination unless you also link
  `fault_handler` (see `AGENTS.md`'s scratch-register table before adding
  one).

## Build
```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras   # needed by i2s_audio
cmake -S examples -B examples-build
cmake --build examples-build
```
Produces `examples-build/uf2/pico_dv_pico1.uf2` (built alongside the other
5 combinations in the same pass -- see [`../examples/CMakeLists.txt`](../examples/CMakeLists.txt)).
To build only this one board manually instead:
```sh
cmake -S examples/pico_dv -B build-pico1 -DPICO_BOARD=pico -DEXAMPLE_OUTPUT_NAME=pico_dv_pico1
cmake --build build-pico1
```

## Example
[`examples/pico_dv/`](../examples/pico_dv/) -- shared verbatim with the Pico
1 W and Pico 2 variants of this same carrier; only `-DPICO_BOARD=pico`
differs.

## Notes/gotchas
- The three Pico-variant docs (this one, `pico_dv_pico1w.md`,
  `pico_dv_pico2.md`) exist because `PICO_BOARD` is the *only* difference
  between them -- same carrier, same pins, same example source.
- `dvi_hdmi`'s pin config in the example (`pimoroni_demo_hdmi_cfg` from
  `common_dvi_pin_configs.h`) is a best-guess placeholder chosen because its
  name matches this carrier family and its pins (TMDS 8/10/12, clk 6) don't
  collide with the validated `kPicoDvCarrier` I2S (26-28) or SD (5/18/19/22)
  pins -- it has **not** been independently confirmed on this carrier the
  way the SD/I2S presets have. Confirm on your hardware, or swap in the
  right constant from that header, before relying on picture output.
- Pico 1 has no wireless -- nothing in this combination uses it either way.
