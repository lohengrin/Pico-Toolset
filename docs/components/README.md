# Components and libraries

One page per driver. All code is in namespace `pico_toolset`, C++20; CMake
targets are `pico_toolset_<name>`; each component has a
`PICO_TOOLSET_BUILD_<NAME>` option (except `driver_interfaces`) and an
`example/` program that doubles as a smoke test.

| Page | Target | Kind | Option default | Summary |
|---|---|---|---|---|
| [driver_interfaces](driver-interfaces.md) | `pico_toolset_driver_interfaces` | interface | always | `DisplayPanel` and `TouchPanel` contracts |
| [ssd1306](ssd1306.md) | `pico_toolset_ssd1306` | display | ON | I2C monochrome OLED |
| [ili9486](ili9486.md) | `pico_toolset_ili9486` | display | ON | 480x320 SPI TFT behind a shift-register bridge |
| [st7789](st7789.md) | `pico_toolset_st7789` | display | ON | ST7789 SPI TFT (CrowPanel, Pico Display Pack) |
| [st7796](st7796.md) | `pico_toolset_st7796` | display | ON | ST7796U 480x320 SPI TFT (SunFounder) |
| [xpt2046](xpt2046.md) | `pico_toolset_xpt2046` | touch | ON | Resistive touch controller + calibration helper |
| [dvi_hdmi](dvi-hdmi.md) | `pico_toolset_dvi_hdmi` | video | ON | PIO DVI/HDMI output + optional HDMI audio |
| [psram](psram.md) | `pico_toolset_psram` | memory | ON (RP2350 only) | External QSPI PSRAM + allocator |
| [sdcard](sdcard.md) | `pico_toolset_sdcard` | storage | ON | FatFs over SD-SPI (PIO or hardware SPI) |
| [flash_store](flash-store.md) | `pico_toolset_flash_store` | storage | ON | Power-fail-safe settings store in flash |
| [usb_hid](usb-hid.md) | `pico_toolset_usb_hid` | USB | ON | PIO-USB host: keyboard, mouse, gamepad |
| [usb_composite](usb-composite.md) | `pico_toolset_usb_composite` | USB | ON | CDC + mass-storage device |
| [i2s_audio](i2s-audio.md) | `pico_toolset_i2s_audio` | audio | **OFF** | I2S DAC output (needs pico-extras) |
| [reset_buttons](reset-buttons.md) | `pico_toolset_reset_buttons` | input | ON | Debounced buttons + tagged watchdog reboot |
| [rgb_led](rgb-led.md) | `pico_toolset_rgb_led` | output | ON | PWM RGB LED |
| [lvgl_display](lvgl-display.md) | `pico_toolset_lvgl_display` | UI glue | **OFF** | LVGL adapter over `DisplayPanel`/`TouchPanel` |
| [screen](screen.md) (lib) | `pico_toolset_screen` | UI | ON | `DisplayDriver`, widgets, `BufferedDisplay` |
| [fault_handler](fault-handler.md) (lib) | `pico_toolset_fault_handler` | diagnostics | ON | Hard-fault reporting across a watchdog reset |

## Layers and dependency rule

`libs/` may depend on `components/`; **never the reverse.**

```
libs/screen  ──►  components/driver_interfaces  ◄──  components/{ili9486,st7789,st7796,xpt2046}
     │                                                        │
     └── adaptors: BufferedDisplay(DisplayPanel&), Ssd1306Driver(Ssd1306&)
components/lvgl_display ──► driver_interfaces (+ LVGL), optional usb_hid
components/usb_composite_hid ──► usb_hid
components/sdcard ──► pico_fatfs      components/usb_hid ──► Pico-PIO-USB, tinyusb_host
components/i2s_audio ──► pico-extras  components/psram ──► hardware_psram (RP2350 only)
```

Two interface layers exist (details in [driver_interfaces](driver-interfaces.md)):
the low-level, hardware-shaped `DisplayPanel`/`TouchPanel` in
`components/driver_interfaces`, and the higher framebuffer-shaped
`DisplayDriver` in `libs/screen`. `init(Config&)` is never part of an interface
because config structs are driver-specific.

## Build options

Top-level options in `CMakeLists.txt` (all `ON` unless noted): `SSD1306`,
`ILI9486`, `ST7789`, `ST7796`, `XPT2046`, `PSRAM`, `USB_HID`, `I2S_AUDIO`
(**OFF**), `USB_COMPOSITE`, `LVGL_DISPLAY` (**OFF**), `SDCARD`, `FLASH_STORE`,
`RGB_LED`, `RESET_BUTTONS`, `DVI_HDMI`, `FAULT_HANDLER`, `SCREEN`,
`SCREEN_PIMORONI` (**OFF**), each as `PICO_TOOLSET_BUILD_<NAME>` (except the
last, `PICO_TOOLSET_SCREEN_PIMORONI`). Component-level options:
`PICO_TOOLSET_SDCARD_STDIO` (OFF), `PICO_TOOLSET_DVI_HDMI_AUDIO` (OFF),
`PICO_TOOLSET_DVI_HDMI_IRQ_STATS` (OFF), `PICO_TOOLSET_USB_HID_KEYMAPS`
(`us;fr`), `PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP` (`us`).

Environment/cache variables: `PICO_SDK_PATH`, `PICO_EXTRAS_PATH`,
`PICO_PIO_USB_DIR`, `PICO_FATFS_DIR`, `PICO_LVGL_DIR`, `LV_CONF_PATH`,
`PIMORONI_PICO_PATH`, `PICO_BOARD_HEADER_DIRS`.

## Watchdog scratch registers

`watchdog_hw->scratch[0..7]` is one repo-wide namespace.

| Index | Owner | Purpose |
|---|---|---|
| 0 | `reset_buttons` | tagged-reboot magic |
| 1 | `reset_buttons` | tag value |
| 2 | `fault_handler` | fault magic |
| 3 | `fault_handler` | faulting PC |
| 4 | pico-sdk | reserved by the SDK's own watchdog bookkeeping |
| 5 | `fault_handler` | faulting LR |
| 6 | `fault_handler` | CFSR |
| 7 | free | -- |

Claim an index only after extending this table, the top-level README table and
the `fault_handler.h` doc comment.

## Presets

Known-good wiring lives in `components/<name>/include/pico_toolset/<name>_configs.h`
as `pico_toolset::configs::<component>::<BoardName>`. Presets are added only for
combinations validated on real hardware. The board-level view is in
[../boards/](../boards/README.md).
