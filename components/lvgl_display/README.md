# LVGL display bridge (`pico_toolset_lvgl_display`)

LVGL v9 (tested with 9.2.2) glue, config-driven like the other components:

- `LvglDisplayAdapter::init(DisplayPanel&, {draw_buffer, pixels})` -- partial
  rendering into your tile buffer, flushed to any SPI TFT driver behind the
  `DisplayPanel` interface (RGB565, byte-swapped to wire order for you);
- `init_framebuffer(fb, w, h, ...)` -- same, but renders into a caller-owned
  native-endian RGB565 framebuffer that something else scans out (e.g. the
  `dvi_hdmi` encoder on another core);
- `add_touch(TouchPanel&, calibration)` -- pointer indev with linear
  calibration (`LvglTouchCalibration`; `swap_axes`, raw min/max);
- `tick()` -- feeds LVGL's tick from the pico clock and runs the timer
  handler; `set_idle_hook()` lets the consumer service something
  time-critical (USB) after every flushed tile.

`pico_toolset_lvgl_hid` (only if `pico_toolset_usb_hid` exists) adds
`lvgl_hid_init(UsbHidHost&)`: keyboard/gamepad -> keypad focus navigation on a
new default `lv_group` (call **before** building widgets), mouse -> pointer
with a crosshair cursor (set `UsbHidConfig::mouse_max_x/y` to the canvas size).

## LVGL and `lv_conf.h`

The toolset does not own LVGL's configuration: memory pool, fonts and widgets
are per-application. Set `LV_CONF_PATH` to your `lv_conf.h`, then
`include(cmake/pico_lvgl.cmake)` (FetchContent v9.2.2, or `PICO_LVGL_DIR`).
Watch for a UTF-8 BOM at the start of `lv_conf.h`. Size matters on small
flash budgets: build with `-Os` and trim widgets/fonts.

There is no standalone example (it needs your `lv_conf.h`); PicoBoot's
`ui/lvgl` and `targets/picoboot_lvgl_{lcd,dvi}` are the reference consumers.
