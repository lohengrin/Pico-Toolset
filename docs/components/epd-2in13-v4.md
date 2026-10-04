# epd_2in13_v4

Waveshare 2.13" V4 e-paper (122x250, 1 bpp) driver plus the vendored Waveshare
GUI_Paint drawing library and fonts.

| | |
|---|---|
| Target / option | `pico_toolset_epd_2in13_v4` / `PICO_TOOLSET_BUILD_EPD_2IN13_V4` (ON) |
| Links | public `pico_stdlib`, `hardware_spi`, `pico_toolset_epd_paint` |
| Example | `components/epd_2in13_v4/example/epd_2in13_v4_example.cpp` |
| Presets | `configs::epd_2in13_v4::kWavesharePicoEpaper213` (Pico-ePaper-2.13 on a Pico/Pico W header) |
| Reference consumer | PicoADSB (Pico W) |

## API

- `Epd2in13V4Config { spi, pin_sck, pin_mosi, pin_cs, pin_dc, pin_rst, pin_busy, spi_freq_hz = 4 MHz }`.
- `Epd2in13V4`: `init(cfg)` (GPIO/SPI only), `init_panel()`, `display()`,
  `display_base()`, `display_partial()`, `clear()`, `sleep()`, and the policy
  helpers `update(fb)` / `clear_screen()`.
- `update(fb)`: first call (and the first after `clear_screen()`) = `init_panel()`
  + `display_base()` (one blink); later calls = `display_partial()` only. Always
  ends with `sleep()`.
- `paint/epd_paint.h`: `extern "C"`-wrapped Waveshare `GUI_Paint` + fonts (target
  `pico_toolset_epd_paint`, usable on its own).

## Constraints

- Framebuffer is the panel's native portrait layout (`kBytesPerRow` x `kHeight`);
  use `Paint_NewImage(fb, kWidth, kHeight, 90, WHITE)` for a landscape canvas
  (use 270 if it appears upside down).
- GUI_Paint quirk: `Paint_DrawString_EN` swaps its colour arguments internally;
  effective order is (cell background, glyph colour), and a WHITE background
  draws glyphs only (transparent).
- Call `clear_screen()` periodically (PicoADSB: every ~17 min) to limit ghosting.
- `wait_busy()` has no timeout (as in Waveshare's driver): a miswired BUSY pin hangs.
- Waveshare's `DEV_Config.h` is replaced by a typedefs-only shim; the pins and SPI
  instance come from the config. See `components/epd_2in13_v4/NOTICE` for the
  provenance and the licence caveat.
