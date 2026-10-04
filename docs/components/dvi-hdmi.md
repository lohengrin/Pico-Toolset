# dvi_hdmi

PIO-based DVI/TMDS serialiser and encoder (no HSTX needed -- works on any GPIO set
a board wires to its connector) with optional HDMI data-island digital audio. It is
the vendored **C API** from Waveshare's RP2350-PiZero example repo, derived from
Wren6991/PicoDVI (BSD-3-Clause), not a C++ class. Provenance, patch tags and
licences are in the component's own
[`README.md`](../../components/dvi_hdmi/README.md) and `LICENSE`.

| | |
|---|---|
| Target / option | `pico_toolset_dvi_hdmi` (**`INTERFACE` library**, so each consumer configures it with compile definitions) / `PICO_TOOLSET_BUILD_DVI_HDMI` (ON) |
| Sub-options | `PICO_TOOLSET_DVI_HDMI_AUDIO` (OFF; adds `audio_ring.cpp`, `data_packet.cpp`, `DVI_ENABLE_AUDIO=1`), `PICO_TOOLSET_DVI_HDMI_IRQ_STATS` (OFF; `DVI_ENABLE_IRQ_STATS=1`) |
| Config defines | `dvi_config_defs.h`: `DVI_VERTICAL_REPEAT`, `DVI_MONOCHROME_TMDS`, ... |
| Links | `pico_base_headers`, `pico_util`, `hardware_dma`, `hardware_interp`, `hardware_pio`, `hardware_pwm` |
| Example | `components/dvi_hdmi/example/dvi_hdmi_example.cpp` -- colour bars; **not run on hardware** |
| Boards | [PiZero](../boards/waveshare-rp2350-pizero.md), [Pico DV](../boards/pimoroni-pico-dv-demo-base.md) |

## API (`dvi.h`)

`dvi_inst` (`timing`, `ser_cfg`, `q_colour_valid`, `q_colour_free`),
`dvi_init(&inst, spin_lock_a, spin_lock_b)`, `dvi_register_irqs_this_core(&inst,
DMA_IRQ_0)`, `dvi_start(&inst)`, `dvi_scanbuf_main_16bpp(&inst)` (never returns;
runs on the core that owns scanout), `dvi_timing_640x480p_60hz`,
`next_striped_spin_lock_num()`, `dvi_irq_us_accum()` (stats).

Typical flow: fill scanline buffers, queue them on `q_colour_valid`, launch core1
running `dvi_register_irqs_this_core()` + `dvi_start()` + `dvi_scanbuf_main_16bpp()`,
then recycle buffers from `q_colour_free`. For GPIO >= 32 call
`pio_set_gpio_base(pio, 16)` first ([PIO window](../guides/pio-gpio-window.md)).

## Pin configurations (`common_dvi_pin_configs.h`)

Plain `static const dvi_serialiser_cfg` constants, **not** in
`pico_toolset::configs`; all use `sm_tmds {0,1,2}`.

| Name | TMDS D0/D1/D2 | CLK | Inverted | Board |
|---|---|---|---|---|
| `pico_sock_cfg` (default) | 36, 34, 32 | 38 | no | **Waveshare RP2350-PiZero onboard connector** (validated) |
| `pimoroni_demo_hdmi_cfg` | 8, 10, 12 | 6 | yes | **Pimoroni Pico DV Demo Base** (validated by PicoBoot on a Pico W) |
| `picodvi_reva_dvi_cfg` | 24, 26, 28 | 22 | yes | PicoDVI Rev A |
| `picodvi_dvi_cfg` | 10, 12, 14 | 8 | yes | PicoDVI Rev C DVI socket |
| `picodvi_pmod0_cfg` | 2, 4, 0 | 6 | no | PicoDVI PMOD0 |
| `amy_dvi_cfg` | 14, 16, 18 | 12 | yes | AMY-DVI |
| `micromod_cfg` | 18, 20, 22 | 16 | yes | SparkX HDMI + MicroMod |
| `adafruit_feather_dvi_cfg` | 18, 20, 22 | 16 | yes | Adafruit Feather RP2040 DVI (hard-codes `pio0`) |
| `not_hdmi_featherwing_cfg` | 11, 9, 7 | 24 | yes | Not-HDMI FeatherWing (hard-codes `pio0`) |
| `waveshare_rp2040_pizero` | 26, 24, 22 | 28 | no | Waveshare RP2040-PiZero |

`DVI_DEFAULT_PIO_INST` defaults to `pio0`; `DVI_DEFAULT_SERIAL_CONFIG` to
`pico_sock_cfg`.

## Requirements and gotchas

- **`clk_sys` exactly 252 MHz, VREG 1.20 V** for 640x480p60 ([clocks](../guides/clocks-and-power.md)).
- **Do not feed `dvi_scanbuf_main_16bpp()` once per frame** -- that gives a permanent
  red screen. Use a persistent framebuffer that core1 re-encodes continuously
  (TOM6809). A 640x240 canvas is output as 640x480 with `DVI_VERTICAL_REPEAT=2`.
- **Core placement:** the per-scanline DMA IRQ cannot share a core with
  Pico-PIO-USB's SOF-timer IRQ ([USB guide](../guides/usb-pio-pitfalls.md)). On RP2040
  it can also starve the SIO FIFO IRQ ([Pico DV doc](../boards/pimoroni-pico-dv-demo-base.md)).
- **Framebuffer in SRAM**, not PSRAM (XIP cache, no cross-core coherency).
- **HDMI audio:** N = 6272, CTS = 28000 for 44.1 kHz at a 25.2 MHz pixel clock, mono
  duplicated to stereo, in data islands (`audio_ring_t`, 44.1 kHz, 8 voices in
  PicoDoom). Do not call `dvi_audio_sample_buffer_set()` before `dvi_init()`.
  `data_packet.h` documents RAM residency requirements for the audio code.
- `dvi_dma_irq_handler()` runs on core1 with zero slack; the optional IRQ-time
  statistic helps measure headroom.
- A PicoBoot Pico DV canvas uses RGB332 to fit RP2040 RAM.
