# PIO GPIO window (pins 32 and above)

On the RP2350B (48 GPIO) each PIO block sees only a **32-pin window** of the
GPIO space: pins 0-31 by default, or 16-47 after `pio_set_gpio_base(pio, 16)`.
A pin number outside the window does not raise an error -- it silently wraps back
into 0-31 and programs the wrong pin.

On the Waveshare RP2350-PiZero this affects:

| Peripheral | Pins | PIO block |
|---|---|---|
| uSD (SD-SPI over PIO) | MISO = 40, CS = 43 (SCK 30, MOSI 31) | PIO1 |
| DVI/HDMI TMDS | 32-39 | PIO0 |
| USB-PIO | 28/29 | inside the default window, no action |

## Two steps are required

1. **Compile time:** build with `PICO_PIO_USE_GPIO_BASE=1`. `cmake/pico_fatfs.cmake`
   sets it as an `INTERFACE` definition on the `pico_fatfs` target, so linking
   `pico_toolset_sdcard` is enough. It is harmless on boards where every pin is
   below 32.
2. **Run time:** call `pio_set_gpio_base(pio, 16)` on the block *before* any
   state machine in it is configured:
   - `SdCardConfig::gpio_base = 16` does this for the SD driver.
   - For DVI call `pio_set_gpio_base(dvi.ser_cfg.pio, 16)` yourself before
     `dvi_init()`; libdvi never does (the example in
     `components/dvi_hdmi/example/` and PicoDoom's `i_video_dvi.cpp` show it).

## Symptoms when a step is missed

| Missed | Symptom |
|---|---|
| Compile-time definition | GPIO40 aliases to GPIO8; the SD mount hangs |
| Run-time call | `pio_sm_set_config()` returns `PICO_ERROR_BAD_ALIGNMENT` **without writing registers**; the vendored code never checks it, so the state machine runs with stale registers. SD: mount times out with `FR_NOT_READY`, program counter wanders in 0-31. DVI: no signal |

## One window per PIO block

All state machines of one block share the base. Do not put a peripheral that
needs pins 0-15 (outside the 16-47 window) in the same block as one that needs
32+. On the PiZero this is why the layout is DVI on PIO0 (32-39 via base 16),
SD on PIO1 (30-43 via base 16) and USB-PIO on PIO2/PIO0 (28/29, both inside
16-47).

## Sources

`components/sdcard` (`gpio_base`), `cmake/pico_fatfs.cmake`, PicoDoom
`AGENTS.md` and `src/i_video_dvi.cpp`, TOM6809 `README-PICO.md` ("PIO beyond
GPIO31").
