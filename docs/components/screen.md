# screen (library)

Header-only display abstraction and widget composition, living in `libs/screen/`
and depending only on `components/driver_interfaces`.

| | |
|---|---|
| Target / option | `pico_toolset_screen` (`INTERFACE`) / `PICO_TOOLSET_BUILD_SCREEN` (ON); `PICO_TOOLSET_SCREEN_PIMORONI` (OFF, needs `PIMORONI_PICO_PATH`) |
| Example | `libs/screen/example/screen_example.cpp` -- HUD on an SSD1306 (built only when `pico_toolset_ssd1306` exists) |

## Pieces

- `Color { rgb565 }` with `from_rgb888`, `inverted()`, `scaled(num, den)`; constants
  `kColorBlack/White/Red/Green/Blue`.
- `DisplayDriver` (`display_driver.h`): pure `width()`, `height()`,
  `set_pixel(x, y, Color)`; virtual with defaults `fill_rect`, `flush`,
  `set_backlight`, `draw_line` (Bresenham), `draw_thick_line`.
- Adaptors:
  - `BufferedDisplay(DisplayPanel&, uint16_t* framebuffer)` -- works with any
    `DisplayPanel` (`Ili9486`, `St7789`, `St7796`); tracks a dirty rectangle and
    `flush()` uploads only those rows via the panel's DMA trio. The framebuffer is
    caller-owned; the internal line-staging buffer is fixed at 480 pixels.
  - `Ssd1306Driver(Ssd1306&)` -- monochrome; any non-zero colour draws the pixel on.
  - `PimoroniDriver(pimoroni::PicoGraphics&)` -- only under `PICO_TOOLSET_SCREEN_PIMORONI`.
- `Screen(DisplayDriver&)`: five slots `UL`, `UR`, `BL`, `BR`, `FS`; `set_widget`,
  `slot_rect`, `clear`, `draw`, `update`, `set_backlight`. `Screen` does **not** own
  widgets.
- Widgets (`widget.h`): `RectWidget`, `TextWidget` (stores a *pointer* to the text;
  `set_text`, `set_position`, `set_color`, `set_transparent`, `text_width`,
  `text_height`, `set_centered`), `BitmapWidget` (pre-decoded RGB565 pixels),
  `BarWidget` (`set_value`, `tick()` returns whether the max marker moved,
  `set_thresholds`, `set_colors`), `HBarWidget`, `LineGraphWidget` (`push_value`,
  `set_autoscale`, `set_title`, `set_value_formatter`, `set_label_color`).
- `simple_font.h`: full 5x8 printable-ASCII font (`kGlyphFont5x8`,
  `glyph_font_height`) including a degree glyph.

## Memory

A caller-owned framebuffer is needed for `BufferedDisplay`: 480x320x2 is about
300 KB (use PSRAM), 320x240x2 is 150 KB (most of an RP2040's SRAM), 240x135x2 is
about 63 KB.

## Gotchas

- Keep widgets and their text buffers alive as long as `Screen` uses them.
- `Ssd1306` has its own drawing primitives that duplicate the generic widgets;
  prefer `Ssd1306Driver` plus widgets for code that should work on any display.
- `PICO_TOOLSET_SCREEN_PIMORONI` is checked by the header but the library's CMake
  does not define it; add the compile definition in your project if you use the
  Pimoroni adaptor.
