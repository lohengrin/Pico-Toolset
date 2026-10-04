# lvgl_display

Glue between LVGL and the toolset's low-level interfaces: an LVGL display bound
to a `DisplayPanel`, touch via `TouchPanel`, GPIO keys as a keypad, and USB HID as
keyboard/mouse. The component's own
[`README.md`](../../components/lvgl_display/README.md) is the long form.

| | |
|---|---|
| Option | `PICO_TOOLSET_BUILD_LVGL_DISPLAY` (**OFF**) |
| Dependencies | LVGL (v9.2.2 fetched by `cmake/pico_lvgl.cmake`, or `PICO_LVGL_DIR`), your own `lv_conf.h` via `LV_CONF_PATH`; fails with a clear error if the `lvgl` target is missing |
| Targets | `pico_toolset_lvgl_display` (`lvgl_display.cpp`, `lvgl_gpio_keys.cpp`; links `pico_stdlib`, `hardware_gpio`, `driver_interfaces`, `lvgl::lvgl`) and `pico_toolset_lvgl_hid` (only if `pico_toolset_usb_hid` exists) |
| Example | none (needs your `lv_conf.h`); see PicoBoot `ui/lvgl` and `targets/picoboot_lvgl_*` |

## API

- `LvglDisplayConfig { uint16_t* draw_buffer; size_t draw_buffer_pixels; }`
- `LvglTouchCalibration { swap_axes = false, raw_h_min = 0, raw_h_max = 4095,
  raw_v_min = 0, raw_v_max = 4095 }`
- `LvglDisplayAdapter`:
  - `init(DisplayPanel&, cfg)` -- calls `lv_init`;
  - `init_framebuffer(uint16_t* fb, w, h, cfg)` -- full RGB565 framebuffer backend;
  - `init_framebuffer_rgb332(uint8_t* fb, w, h, cfg)` -- RGB332 backend with dithered
    RGB565 -> RGB332 conversion (PicoBoot's Pico DV canvas; whites look slightly
    yellow);
  - `add_touch(TouchPanel&, cal)`; `tick()`; `set_idle_hook(void(*)())`;
    `static set_flush_tap(FlushTap, ctx)` (observe rendered tiles, e.g. screenshots);
    `display()`.
- `lvgl_gpio_keys.h`: `LvglGpioKey { pin, lv_key }`,
  `LvglGpioKeyPolarity { kActiveLow, kActiveHigh, kAuto }`,
  `LvglGpioKeysConfig { keys, key_count, polarity = kActiveLow }`,
  `lvgl_gpio_keys_init(cfg)`. `kAuto` detects polarity from an idle pull-up/pull-down
  pair changing, independent of wiring (used for the Pico DV buttons).
- `lvgl_hid.h`: `lvgl_hid_init(UsbHidHost&)`.

## Notes

- LVGL buffers are native-endian but ILI9486/ST7796 want big-endian on the wire;
  skipping the swap gives wrong colours.
- Touch samples are mapped with `LvglTouchCalibration`; see
  [xpt2046](xpt2046.md) for panel values.
