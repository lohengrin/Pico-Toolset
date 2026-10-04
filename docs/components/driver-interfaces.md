# driver_interfaces

Header-only, dependency-free interfaces that let callers swap display or touch
chips without touching call sites. Target `pico_toolset_driver_interfaces`
(`INTERFACE`, links `hardware_spi`); always built, no option, no example.

## Why two interface layers

| Layer | Where | Shape | Implementers |
|---|---|---|---|
| Low-level, hardware-shaped | `components/driver_interfaces` | windowed/DMA pixel streaming (`DisplayPanel`), raw touch sampling (`TouchPanel`) | `Ili9486`, `St7789`, `St7796` (`DisplayPanel`); `Xpt2046Touch` (`TouchPanel`) |
| Higher, framebuffer-shaped | `libs/screen` (`DisplayDriver`) | `set_pixel`/`fill_rect`/`flush`, widgets | `BufferedDisplay` (wraps any `DisplayPanel&`), `Ssd1306Driver`, `PimoroniDriver` |

A component never includes anything from `libs/`. SSD1306 (I2C, 1 bpp, no
windowed/DMA concept) does not implement `DisplayPanel`; it reaches
`DisplayDriver` only through the `Ssd1306Driver` adaptor.

## `DisplayPanel` (`display_panel.h`)

| Member | Kind | Notes |
|---|---|---|
| `int width() const`, `int height() const` | pure | |
| `void set_window(x0, y0, x1, y1)` | pure | inclusive; keeps CS asserted -- caller **must** call `end_write()`; no other bus user may transfer meanwhile |
| `void write_pixels(std::span<const uint16_t>)` | pure | RGB565, big-endian on the wire; blocks until done |
| `void fill_solid(uint16_t rgb565)` | pure | |
| `start_pixels_dma(span)` / `pixels_busy()` / `finish_pixels_dma()` | default | non-blocking trio; default = synchronous `write_pixels`, never busy, no-op finish. Usage: start, poll `pixels_busy()` doing other work, then finish -- before the next `set_window`/`write_pixels`/`end_write` |
| `end_write()` | default no-op | releases CS |
| `set_backlight(uint8_t)` | default no-op | 0-255 |
| `spi_inst_t* spi() const` | default `nullptr` | bus-sharing accessor for a co-located touch/SD driver |

## `TouchPanel` (`touch_panel.h`)

```cpp
struct TouchSample { bool pressed = false; uint16_t raw_x = 0, raw_y = 0; };
class TouchPanel { virtual TouchSample read() = 0; };
```

Shared-bus contract: an implementer never calls `spi_init()`/`i2c_init()`; it
uses whatever bus the caller (usually the display driver) brought up.

## Rules for adding an interface

Extract a kind interface only when a second implementation appears or a
consumer needs to swap one. Pure-virtual essentials plus virtual-with-default
conveniences. `init(Config&)` stays outside the interface. Update
[`AGENTS.md`](../../AGENTS.md) "Kind interfaces" when you do.

## Usage

```cpp
pico_toolset::St7789 lcd;  lcd.init(pico_toolset::configs::st7789::kElecrowCrowPanelPicoHmi28);
pico_toolset::DisplayPanel& panel = lcd;      // everything after init goes through the interface
panel.set_window(0, 0, 319, 239);
panel.write_pixels(pixels); panel.end_write();
```

Client usage: PicoDoom (`g_panel` in `i_video_ili9486.cpp`), TOM6809
(`LcdRenderer` takes `DisplayPanel&`), PiCoMonitor (`BufferedDisplay`).

