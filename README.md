# Pico-Toolset

Reusable, configuration-driven drivers for Raspberry Pi Pico (RP2040 and
RP2350), extracted from real-hardware consumer projects and
re-packaged as a set of independent CMake components. Every component is
pin/config-structured so it adapts to any board wiring, ships with an example
that doubles as an integration smoke-test, and can be built or skipped
independently.

## Components

| Component | Target | Description |
|-----------|--------|-------------|
| Driver interfaces | `pico_toolset_driver_interfaces` | Header-only, dependency-free `DisplayPanel`/`TouchPanel` interfaces -- the low-level windowed/DMA-streaming and raw-touch-sampling contracts `ILI9486`/`ST7789`/`ST7796U`/`XPT2046` implement so callers can swap chips without rewriting call sites. See AGENTS.md's "Kind interfaces". |
| SSD1306   | `pico_toolset_ssd1306`  | I2C monochrome OLED driver (128x64/128x32/...) with framebuffer, BMP blitting, basic primitives. |
| ILI9486   | `pico_toolset_ili9486`  | 480x320 SPI TFT driver for Waveshare-style boards where the panel sits behind a 16-bit shift register; DMA-backed pixel streaming, backlight PWM. |
| ST7789    | `pico_toolset_st7789`   | ST7789-family SPI TFT driver (e.g. the CrowPanel PICO HMI 2.8"'s 320x240 panel); config-driven geometry/MADCTL/inversion, DMA-backed pixel streaming, backlight PWM. |
| ST7796U   | `pico_toolset_st7796`   | ST7796U SPI TFT driver (e.g. a 480x320 Waveshare-wiring-compatible panel replacing an ILI9486 board); direct SPI (no shift-register bridge), config-driven geometry/MADCTL, live SPI-clock tuning, DMA-backed pixel streaming, backlight PWM. |
| XPT2046   | `pico_toolset_xpt2046`  | Resistive touch controller sharing an SPI bus with a display driver (e.g. ILI9486/ST7796U); raw ADC reads + a linear calibration helper. |
| PSRAM     | `pico_toolset_psram`    | RP2350-only external PSRAM bring-up (QMI CS1), self-test, free-list allocator, `std::pmr` adapter. No-op on RP2040. |
| USB HID   | `pico_toolset_usb_hid`  | PIO-USB TinyUSB host: keyboard/mouse/HID-gamepad + XInput + DualSense (VID/PID-detected), double-buffered cross-core state, unified `GamepadState`. |
| I2S audio | `pico_toolset_i2s_audio` | Float-sample I2S DAC output (e.g. PCM5100A) via pico-extras' `pico_audio_i2s`; non-blocking queue, config-driven pins/DMA channel/PIO SM. OFF by default -- needs pico-extras set up by the consumer (see below). |
| SD card   | `pico_toolset_sdcard`   | FatFs R0.15 (elehobica/pico_fatfs) over native or PIO-bit-banged SPI; config-driven pins/PIO/gpio_base, `list_files()`/`read_file()`/`read_file_pmr()`. |
| USB composite | `pico_toolset_usb_composite` (+ `_hid` variant) | TinyUSB *device* composite on the native port: CDC serial (optional stdio driver) + MSC over a caller-supplied block device + the picotool vendor reset interface (reboot to BOOTSEL / flash without the button). A `_hid` variant shares one `tusb_config.h` with `usb_hid` for firmware that is device *and* PIO-USB host. See its README. |
| LVGL display | `pico_toolset_lvgl_display` (+ `pico_toolset_lvgl_hid`) | LVGL v9 bridge: any `DisplayPanel` (SPI TFTs) or a caller-owned RGB565 framebuffer (DVI scanout) as an LVGL display, `TouchPanel` as a pointer, and (optional `_hid` target) USB keyboard/mouse/gamepad as keypad + pointer. OFF by default (needs LVGL via `cmake/pico_lvgl.cmake` and the consumer's `lv_conf.h`). See its README. |
| Reset buttons | `pico_toolset_reset_buttons` | N debounced, active-HIGH momentary buttons + a generic tagged-watchdog-reboot pair (`watchdog_reboot_with_tag()`/`consume_pending_watchdog_tag()`), reusable for any "boot straight into mode X" use case. |
| DVI/HDMI  | `pico_toolset_dvi_hdmi` | PIO-based DVI/TMDS serialiser + encoder (no HSTX needed -- works on any GPIO set a board wires to its connector), with optional HDMI data-island digital audio (CEA-861 InfoFrames/ACR/audio-sample packets) and a core1 IRQ-handler-headroom measurement tool. See "Credits and third-party code" below for full provenance. |

## Libraries

Header-only helpers that sit above the components rather than being
standalone drivers.

| Library | Target | Description |
|---------|--------|-------------|
| Screen   | `pico_toolset_screen`  | Pluggable `DisplayDriver` abstraction + slot-based widget composition (`RectWidget`/`TextWidget`/`BitmapWidget`/`BarWidget`/`HBarWidget`/`LineGraphWidget`). `BufferedDisplay` adapts any `DisplayPanel` (ILI9486/ST7789) + a caller-owned framebuffer; `Ssd1306Driver`/`PimoroniDriver` adapt those directly. |
| Fault handler | `pico_toolset_fault_handler` | Cortex-M33 hard-fault handler that survives a watchdog reset to report PC/LR/CFSR on the *next* boot, instead of the SDK's default silent halt. No board wiring involved (core MCU + watchdog only), hence a library rather than a `components/` driver. |

All code lives in namespace `pico_toolset` and targets C++20.

## Layout

```
pico-toolset/
├── CMakeLists.txt            # top-level: options + subdirectories
├── boards/                   # board/combination docs + pico-sdk board-definition headers
│   ├── README.md
│   └── waveshare_rp2350_pizero.h
├── examples/                 # full-combination examples, one per board/combo (see boards/)
│   ├── CMakeLists.txt        # superbuild: builds every combination in one pass
│   ├── pico_dv/
│   ├── waveshare_pizero/
│   └── waveshare_pizero_lcd35a/
├── cmake/
│   ├── pico-toolset.cmake    # helper for FetchContent consumers
│   └── pico_pio_usb.cmake    # makes Pico-PIO-USB available (submodule or fetch)
├── components/
│   ├── driver_interfaces/ include/pico_toolset/{display_panel.h,touch_panel.h}  (header-only)
│   ├── ssd1306/   include/pico_toolset/ssd1306.h   src/  example/
│   ├── ili9486/   include/pico_toolset/ili9486.h   src/  example/
│   ├── st7789/    include/pico_toolset/st7789.h    src/  example/
│   ├── st7796/    include/pico_toolset/st7796.h    src/  example/
│   ├── xpt2046/   include/pico_toolset/*.h         src/  example/
│   ├── psram/     include/pico_toolset/psram.h     src/  example/
│   ├── i2s_audio/ include/pico_toolset/i2s_audio.h src/  example/
│   ├── sdcard/    include/pico_toolset/sdcard.h    src/  example/
│   ├── reset_buttons/ include/pico_toolset/reset_buttons.h src/ example/
│   ├── usb_hid/   include/pico_toolset/*.h  src/  example/  tusb_config.h
│   ├── usb_composite/ include/  config/tusb_config.h  src/  example/
│   ├── lvgl_display/  include/pico_toolset/{lvgl_display,lvgl_hid}.h  src/
│   └── dvi_hdmi/  dvi.h dvi.c ... (flat, vendored -- see its own README.md)
└── libs/
    ├── screen/         include/pico_toolset/*.h  example/
    └── fault_handler/  include/pico_toolset/fault_handler.h  src/  example/
```

## Building (in-tree)

Requires the pico-sdk `PICO_SDK_PATH` and a working RP2040/RP2350 CMake
toolchain (`PICO_BOARD` for the RP2350, e.g. `pico2`).

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
cmake -B build -DPICO_BOARD=pico2
cmake --build build
```

Both RP2040 (`pico`) and RP2350 (`pico2`) boards are supported. PSRAM is
compiled only for RP2350. USB HID needs Pico-PIO-USB (see below).

### Build options

| Option | Default | Meaning |
|--------|---------|---------|
| `PICO_TOOLSET_BUILD_SSD1306` | ON | Build SSD1306 driver + example |
| `PICO_TOOLSET_BUILD_ILI9486` | ON | Build ILI9486 driver + example |
| `PICO_TOOLSET_BUILD_ST7789`  | ON | Build ST7789 driver + example |
| `PICO_TOOLSET_BUILD_ST7796`  | ON | Build ST7796U driver + example |
| `PICO_TOOLSET_BUILD_XPT2046` | ON | Build XPT2046 touch driver + example |
| `PICO_TOOLSET_BUILD_PSRAM`   | ON | Build PSRAM driver + example (RP2350 only) |
| `PICO_TOOLSET_BUILD_USB_HID` | ON | Build PIO-USB HID host + example |
| `PICO_TOOLSET_BUILD_I2S_AUDIO` | OFF | Build I2S audio output + example (needs pico-extras, see below) |
| `PICO_TOOLSET_BUILD_USB_COMPOSITE` | ON | Build the USB CDC+MSC+reset composite device + RAM-disk example |
| `PICO_TOOLSET_BUILD_LVGL_DISPLAY` | OFF | Build the LVGL bridge (set `LV_CONF_PATH` first) |
| `PICO_TOOLSET_BUILD_SDCARD`  | ON | Build SD card (FatFs/pico_fatfs) driver + example |
| `PICO_TOOLSET_SDCARD_STDIO`  | OFF | Also install POSIX/stdio newlib syscalls (`fopen`/`fread`/...) over FatFs |
| `PICO_TOOLSET_BUILD_RESET_BUTTONS` | ON | Build debounced-buttons + tagged-watchdog-reboot helper + example |
| `PICO_TOOLSET_BUILD_DVI_HDMI` | ON | Build PIO-based DVI/HDMI video + example |
| `PICO_TOOLSET_DVI_HDMI_AUDIO` | OFF | Enable HDMI data-island digital audio (see `components/dvi_hdmi/README.md`) |
| `PICO_TOOLSET_DVI_HDMI_IRQ_STATS` | OFF | Enable core1 IRQ-handler-headroom stats (bring-up/measurement tool) |
| `PICO_TOOLSET_BUILD_FAULT_HANDLER` | ON | Build the hard-fault handler + example |
| `PICO_TOOLSET_USB_HID_KEYMAPS` | `us;fr` | Semicolon-separated keyboard layouts to compile in (implemented: `us`, `fr`) |
| `PICO_TOOLSET_USB_HID_DEFAULT_KEYMAP` | `us` | Layout used by default (must be listed in `PICO_TOOLSET_USB_HID_KEYMAPS`) |
| `PICO_TOOLSET_BUILD_SCREEN`  | ON | Build screen abstraction + example |
| `PICO_TOOLSET_SCREEN_PIMORONI` | OFF | Compile the Pimoroni PicoGraphics backend adaptor |

## Pico-PIO-USB (USB HID component)

The USB HID host runs TinyUSB's host stack over Pico-PIO-USB. Two things must
be available alongside pico-sdk:

1. **The pico-sdk `tinyusb` submodule** (its `tinyusb_host` target). If you
   cloned pico-sdk without submodules, run:

   ```sh
   git -C $PICO_SDK_PATH submodule update --init lib/tinyusb
   ```

2. **Pico-PIO-USB**. Either check it out and point at it (no submodules
   needed on current `main`):

   ```sh
   git clone https://github.com/sekigon-gonnoc/Pico-PIO-USB
   cmake -B build -DPICO_PIO_USB_DIR=/path/to/Pico-PIO-USB
   ```

   or let CMake fetch it:

   ```sh
   cmake -B build   # FetchContent clones Pico-PIO-USB
   ```

Note: with any USB HID build the RP2350 still uses `OPT_MCU_RP2040` internally
(the vendored TinyUSB has no RP2350 MCU port), and rhport 0 stays a USB-CDC
device so `pico_stdio_usb` keeps working alongside the PIO host on rhport 1.

## I2S audio (pico-extras)

`pico_toolset_i2s_audio` wraps pico-extras' `pico_audio_i2s`, which -- like
pico-sdk itself -- must be imported via its own `pico_extras_import.cmake`
**before** your project's `project()` call. This component cannot set that
up itself (it only runs after `project()`, when the toolset is
`add_subdirectory()`'d), so the consumer owns it:

```cmake
set(PICO_SDK_PATH /path/to/pico-sdk)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
set(PICO_EXTRAS_PATH /path/to/pico-extras)   # or PICO_EXTRAS_FETCH_FROM_GIT=ON
include(${CMAKE_CURRENT_SOURCE_DIR}/pico_extras_import.cmake)  # your own copy of pico-extras' script

project(my_app C CXX ASM)
pico_sdk_init()

set(PICO_TOOLSET_BUILD_I2S_AUDIO ON CACHE BOOL "" FORCE)  # OFF by default
add_subdirectory(third_party/pico-toolset)
```

`components/i2s_audio/CMakeLists.txt` fails loud with a clear message
(`pico_toolset_i2s_audio requires the pico_audio_i2s target...`) if
`pico_audio_i2s` doesn't exist by the time it's reached, rather than
misbehaving silently.

## Boards & full-combination examples

`boards/*.md` documents complete assembled board/combinations (which
components to enable, which preset to call, resource conflicts, build
command) -- see [`boards/README.md`](boards/README.md) for the index and
template. `examples/` holds one full-integration example per combination,
all buildable in a single pass with no flags:

```sh
export PICO_SDK_PATH=/path/to/pico-sdk
export PICO_EXTRAS_PATH=/path/to/pico-extras   # needed by the pico_dv legs
cmake -S examples -B examples-build
cmake --build examples-build
```

produces every combination's `.uf2` under `examples-build/uf2/`.

## Using a component from your own project

### FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(pico_toolset
    GIT_REPOSITORY https://github.com/you/pico-toolset.git GIT_TAG main)
FetchContent_MakeAvailable(pico_toolset)
# pick components...
if(PICO_TOOLSET_BUILD_USB_HID)
    FetchContent_MakeAvailable(pico_pio_usb)   # or set PICO_PIO_USB_DIR
endif()
```

`cmake/pico-toolset.cmake` contains the canonical consumer snippet.

### Git submodule

Add it as a submodule, then in your `CMakeLists.txt`:

```cmake
set(PICO_TOOLSET_BUILD_ILI9486 ON  CACHE BOOL "" FORCE)
add_subdirectory(third_party/pico-toolset)
target_link_libraries(my_app PRIVATE pico_toolset_ili9486)
```

### Shared board headers

`boards/` at the repo root holds `pico_sdk` board-definition headers for
carrier boards more than one project targets (currently
`waveshare_rp2350_pizero.h`), so they're defined once instead of vendored
per-project. Point `PICO_BOARD_HEADER_DIRS` at it **before**
`pico_sdk_import.cmake` runs (board selection happens at include time):

```cmake
set(PICO_BOARD waveshare_rp2350_pizero CACHE STRING "Board type" FORCE)
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pico-toolset/boards CACHE STRING "" FORCE)
include(pico_sdk_import.cmake)
```

## Minimal usage

None of these config structs have a working default -- pins, bus instances,
and PIO/DMA assignments are board wiring, not driver behavior, so a
default-constructed config is deliberately not something that "just happens
to work". Each component instead ships a `<name>_configs.h` header with
named presets for the specific board+device combinations this toolset has
validated on real hardware; start from one of those (copying and adjusting
what differs for a similar-but-not-identical board), or fill in every field
yourself for a board without a preset yet -- see each struct's own doc
comment for which fields are required.

### SSD1306

No validated preset exists yet for this component (contribute one once
you've tested it on real hardware) -- fill in every field:

```cpp
pico_toolset::Ssd1306Config cfg;
cfg.i2c_instance = i2c1; cfg.sda_pin = 19; cfg.scl_pin = 18;
pico_toolset::Ssd1306 oled;
oled.init(cfg);
oled.clear();
oled.draw_string(0, 0, 1, "Hello");
oled.show();
```

### ILI9486

```cpp
#include "pico_toolset/ili9486_configs.h"

pico_toolset::Ili9486 lcd;
lcd.init(pico_toolset::configs::ili9486::kWaveshareRp2350PiZero);
lcd.fill_solid(0x001F);                 // solid blue
lcd.set_window(0, 0, 100, 100);
lcd.write_pixels(span_of_100x100_pixels);
lcd.end_write();
```

### ST7789

Two presets exist (`kElecrowCrowPanelPicoHmi28`, `kPimoroniPicoDisplayPack`)
-- pick the one matching your board, or copy and adjust:

```cpp
#include "pico_toolset/st7789_configs.h"

pico_toolset::St7789 lcd;
lcd.init(pico_toolset::configs::st7789::kElecrowCrowPanelPicoHmi28);
lcd.fill_solid(0x001F);                 // solid blue
lcd.set_window(0, 0, 100, 100);
lcd.write_pixels(span_of_100x100_pixels);
lcd.end_write();
```

### ST7796U

```cpp
#include "pico_toolset/st7796_configs.h"

pico_toolset::St7796 lcd;
lcd.init(pico_toolset::configs::st7796::kWaveshareRp2350PiZero);
lcd.fill_solid(0x001F);                 // solid blue
lcd.set_window(0, 0, 100, 100);
lcd.write_pixels(span_of_100x100_pixels);
lcd.end_write();

// Live clock tuning once the panel's real corruption ceiling is known --
// spec ceiling is 125MHz on the Waveshare-wiring-compatible board this was
// validated on, but start conservative (the preset's default) and raise it
// bench-tested, not from the datasheet alone.
lcd.set_pixel_clock_hz(50'000'000);
```

### XPT2046 touch (shares a bus with a display driver)

```cpp
#include "pico_toolset/ili9486_configs.h"
#include "pico_toolset/xpt2046_configs.h"

pico_toolset::Ili9486 lcd;              // spi_init()s spi1 -- must happen first
lcd.init(pico_toolset::configs::ili9486::kWaveshareRp2350PiZero);

auto touch_cfg = pico_toolset::configs::xpt2046::kWaveshareRp2350PiZero;
touch_cfg.spi_instance = lcd.spi();     // always take this from the display driver, not the preset
pico_toolset::Xpt2046Touch touch;
touch.init(touch_cfg);                  // does NOT call spi_init() -- shares lcd's bus

auto sample = touch.read();             // {pressed, raw_x, raw_y} -- polls IRQ first, cheap when idle
if (sample.pressed) {
    pico_toolset::Xpt2046Calibration cal;   // measure your own panel's corners, see its doc comment
    double x = pico_toolset::touch_calibration_linear_map(sample.raw_y, cal.raw_h_min, cal.raw_h_max, 0.0, 479.0);
    double y = pico_toolset::touch_calibration_linear_map(sample.raw_x, cal.raw_v_min, cal.raw_v_max, 0.0, 319.0);
}
```

### PSRAM (RP2350)

```cpp
#include "pico_toolset/psram_configs.h"

auto st = pico_toolset::psram_init(pico_toolset::configs::psram::kWaveshareRp2350PiZero);
void* p = pico_toolset::psram_malloc(4096);
pico_toolset::PsramResource res;        // std::pmr::memory_resource
std::pmr::vector<uint8_t> big(&res);
```

`psram_qspi_sweep_example` (`pico_toolset_psram`, RP2350 only) finds the
highest safe clock for the flash + PSRAM QSPI bus: it raises `clk_sys` one
PLL-attainable step at a time, re-tunes the PSRAM M1 window per step with
`psram_set_clock_hz()`, and verifies both chips' XIP windows with
cache-coherent pattern tests (the RP2350 XIP cache is not read-only and will
lie to you if left alone). It is built with `copy_to_ram` so a step that
breaks QSPI is reported and rolled back instead of crashing the CPU on a bad
instruction fetch. Sweep range/margin live in its `SweepConfig`; the board's
`cs_pin` still comes from `configs::psram::*`.

### USB HID host

```cpp
#include "pico_toolset/usb_hid_configs.h"

// Three presets exist for this board -- pick the one matching your build
// (see usb_hid_configs.h: pio_num/run_on_core1 depend on what else is
// active, and whether you launch core1 yourself).
pico_toolset::UsbHidHost usb;
usb.init(pico_toolset::configs::usb_hid::kWaveshareRp2350PiZeroLcd);
while (true) {
    for (char ch; (ch = usb.consume_typed_ascii_char()) != 0;) putchar(ch);
    auto g = usb.gamepad_state(0);
    auto m = usb.mouse_state();       // clamped absolute cursor (UI use)
    int dx, dy;
    usb.consume_mouse_delta(dx, dy);  // raw unclamped relative movement (mouselook/aim)
}
```

The typed-ASCII queue (`consume_typed_ascii_char()`) interprets each physical
key with a compile-time-selected *keymap*. Layouts are registered in
`kKeymaps` (see `pico_toolset/usb_hid_keymap.h`); which ones ship is chosen by
`PICO_TOOLSET_USB_HID_KEYMAPS`, and the runtime selection lives in
`UsbHidConfig::keymap_index`:

```cpp
cfg.keymap_index = pico_toolset::kDefaultKeymapIndex;   // build default
if (auto* fr = pico_toolset::keymap_by_name("fr"))      // switch at runtime
    cfg.keymap_index = static_cast<uint8_t>(fr - pico_toolset::kKeymaps);
```

Each layout covers a full PC 105 keyboard (letter/digit rows, all punctuation,
the ISO extra key, and the layout-independent keypad). US is QWERTY; FR is the
standard AZERTY layout, where Shift is also what produces the digits. Adding a
new layout = implement it under a `#if PICO_TOOLSET_USB_HID_KEYMAP_*` guard in
`usb_hid_keymap.cpp` and register its name in the component's
`PICO_TOOLSET_USB_HID_KEYMAP_IMPL` list.

NumLock/CapsLock/ScrollLock are tracked and pushed to every mounted
keyboard's own LEDs (`Set_Report(Output)`, boot-keyboard convention) --
`UsbHidHost::numlock_on()`/`capslock_on()`/`scrolllock_on()` are also the only
way to read NumLock's state back, since USB HID gives no other way to query
it. A freshly-mounted keyboard plays a NumLock->CapsLock->ScrollLock->
CapsLock->NumLock->off identify animation before settling into the real,
managed state (NumLock on by default -- see `UsbHidConfig::
numlock_initial_state`/`led_boot_animation`).

### I2S audio (pico-extras -- see the setup section above)

```cpp
#include "pico_toolset/i2s_audio_configs.h"

pico_toolset::I2sAudioOutput audio;
if (!audio.init(pico_toolset::configs::i2s_audio::kPicoDvCarrier)) { /* handle failure */ }
std::array<float, 882> frame;           // [-1, 1] samples, e.g. one 20ms frame at 44.1kHz
// ...fill frame...
audio.queue_samples(frame);             // non-blocking; drops this call's audio if no buffer is free
```

### SD card

```cpp
#include "pico_toolset/sdcard_configs.h"

// Three presets exist -- pick the one matching your board, or copy and adjust.
pico_toolset::SdCard sd;
if (!sd.init(pico_toolset::configs::sdcard::kPicoDvCarrier)) { /* no card / mount failed -- sd.last_mount_result() has the FRESULT */ }
for (const auto& name : sd.list_files({"txt", "bin"})) { /* ... */ }
std::vector<uint8_t> data = sd.read_file("config.bin");
```

`pico_fatfs` is fetched via `FetchContent` by default; set `PICO_FATFS_DIR`
to point at a local checkout instead (same pattern as `PICO_PIO_USB_DIR`
above).

#### POSIX/stdio file access

`list_files()`/`read_file()` load a whole file into memory. If your code
(or a library it uses, e.g. a WAD/archive format with a seekable directory
table) instead calls `fopen()`/`fread()`/`fseek()` or the raw POSIX
`open()`/`read()`/`lseek()`, set `PICO_TOOLSET_SDCARD_STDIO=ON` to also
compile in a newlib syscall shim over the same FatFs mount:

```cpp
pico_toolset::SdCard sd;
sd.init(pico_toolset::configs::sdcard::kWaveshareRp2350PiZero);
// ...then anywhere in the program:
FILE* f = fopen("LEVEL1.DAT", "rb");
```

This overrides newlib's *global* weak file syscalls, so it's opt-in
(default OFF) rather than always built into `pico_toolset_sdcard` --- only
one thing in a given program should own them. Console fds 0/1/2 still pass
through to `pico_stdio` unchanged.

### Reset buttons (debounced + tagged watchdog reboot)

```cpp
uint32_t tag = 0;
if (pico_toolset::consume_pending_watchdog_tag(tag)) {
    // this boot was triggered by watchdog_reboot_with_tag(tag) below
}

pico_toolset::DebouncedButtons buttons;
buttons.init(std::array<uint8_t, 3>{14, 15, 16});  // active-HIGH, pull-down
// ...call buttons.poll() once per frame...
int pressed = buttons.poll();
if (pressed >= 0) {
    pico_toolset::watchdog_reboot_with_tag(static_cast<uint32_t>(pressed));  // never returns
}
```

### Fault handler (hard-fault survives to next boot)

Link `pico_toolset_fault_handler` and call `report_pending_hard_fault()`
once, early in `main()` (after `stdio_init_all()` and any USB-CDC-attach
wait). No other init call is needed -- linking the library alone overrides
the SDK's default `isr_hardfault`.

```cpp
#include "pico_toolset/fault_handler.h"

int main() {
    stdio_init_all();
    sleep_ms(2000);  // let a USB-CDC terminal attach
    pico_toolset::report_pending_hard_fault("MyApp");
    // ...rest of your app...
}
```

`consume_pending_hard_fault(FaultInfo&)` is available if you'd rather act on
the PC/LR/CFSR programmatically than have them printed.

### DVI/HDMI video (+ optional digital audio)

Lower-level than this toolset's other drivers -- no config struct, no C++
class: it's the vendored `dvi.h` C API (PIO serialiser, TMDS encode,
scanline timing), config-driven the way its own upstream already is
(`dvi_inst`/`dvi_serialiser_cfg`, `common_dvi_pin_configs.h`'s named board
pinouts). See `components/dvi_hdmi/README.md` for the full API and its
`example/` for a minimal (not real-hardware-tested by this toolset --
see that file's own header) scanline-based video example.

```cpp
#include "dvi.h"
#include "common_dvi_pin_configs.h"

dvi_inst dvi;
dvi.timing = &dvi_timing_640x480p_60hz;
dvi.ser_cfg = pico_sock_cfg;  // or another common_dvi_pin_configs.h preset
dvi_init(&dvi, next_striped_spin_lock_num(), next_striped_spin_lock_num());
// ...register IRQs + dvi_start() on whichever core owns scanout, feed
// dvi.q_colour_valid/q_colour_free -- see components/dvi_hdmi/README.md.
```

### Screen (widget composition)

```cpp
pico_toolset::Ssd1306 oled; oled.init(cfg);
pico_toolset::Ssd1306Driver drv(oled);
pico_toolset::Screen screen(drv);
pico_toolset::TextWidget t(4, 4, "CPU: 012%", {0xFFFF}, {0x0000},
                           pico_toolset::kGlyphFont5x8.glyphs, 8);
screen.set_widget(pico_toolset::Screen::UL, &t);
screen.update();
```

## Notes and gotchas

- **PSRAM init can hang if it races an interrupt or the other core (real
  hardware finding, 2026-09).** Symptom: an intermittent
  freeze right at PSRAM init -- worse right after flashing, "usually" cleared
  by a reset (sometimes needing several). Root cause, straight from
  `hardware/psram.h`'s own doc comment: `psram_detect_cs_and_size()` and
  `psram_reinitialize()` are documented *unsafe* unless interrupts are
  disabled and the other core is not concurrently executing from flash/PSRAM
  -- both functions briefly switch the QMI into a raw command/direct mode
  where flash is not readable via XIP at all, so any code fetch from flash
  during that window (an ISR firing -- e.g. stdio_usb/TinyUSB's, present in
  every consumer of this toolset -- or the other core mid-instruction-fetch)
  hangs or faults. `pico_toolset_psram` now protects `psram_init()` with
  `flash_safe_execute()` when the other core is lockout-ready
  (`multicore_lockout_ready()`), falling back to a plain interrupt-disable
  otherwise -- **call `psram_init()` before launching any core1 workload**
  for the fallback to be fully sufficient (nothing is running on the other
  core yet, so there's nothing to race).
  **`pico_toolset_usb_hid` deliberately does NOT register its core1 as a
  lockout victim** (tried, reverted): `multicore_lockout_victim_init()`
  installs an IRQ handler that silently steals every word off the raw
  inter-core FIFO, which breaks any consumer -- this toolset's own
  examples included -- that also uses
  `multicore_fifo_push_blocking()`/`pop_blocking()` directly on that core
  (e.g. for a chunked display-blit handoff). Confirmed on real hardware:
  registering the lockout victim made the *display* hang a few frames in
  (the blit-index handoff got silently eaten by the lockout IRQ) even though
  it fixed the original PSRAM freeze. If you need PSRAM/flash operations
  *after* a core1 workload is already running, either reorder your boot so
  PSRAM/flash init happens first (the tested, working pattern), or -- only
  if that core1 loop doesn't use the raw FIFO for anything of its own --
  have it call `flash_safe_execute_core_init()` itself.
- **Watchdog scratch registers are shared across components.**
  `watchdog_hw->scratch[0..7]` is one flat set of 8 words for the whole
  toolset, not per-component storage -- a consumer linking both
  `reset_buttons` and `fault_handler` (or a future component that also needs
  to survive a `watchdog_reboot()`) must not have them collide. Current
  allocation:

  | Index | Owner | Purpose |
  |-------|-------|---------|
  | `scratch[0]` | reset_buttons | tagged-reboot magic |
  | `scratch[1]` | reset_buttons | tagged-reboot tag value |
  | `scratch[2]` | fault_handler | fault magic |
  | `scratch[3]` | fault_handler | faulting PC |
  | `scratch[4]` | *(reserved by the SDK's own watchdog bookkeeping)* | -- |
  | `scratch[5]` | fault_handler | faulting LR |
  | `scratch[6]` | fault_handler | CFSR |
  | `scratch[7]` | *(free)* | -- |

  Adding a new scratch-register user: pick an unclaimed index above, extend
  this table, and mirror it in that component's header doc comment (see
  `libs/fault_handler/include/pico_toolset/fault_handler.h`).
- **ILI9486 wire protocol**: commands go out byte-at-a-time with CS pulsed
  around *every* command/parameter word (16-bit, `0x00`-padded, D/C high);
  pixel data streams as one continuous CS-low burst. `spi_set_baudrate()` is
  called *before* CS is asserted for the window so the peripheral's
  re-initialization can't glitch the shift register.
- **ILI9486 + a shared-bus device (e.g. XPT2046)**: `write_pixels()`'s DMA
  path (`use_dma = true`, the default) drains the SPI RX FIFO and waits out
  the BSY flag after the DMA engine finishes, exactly matching
  `spi_write_blocking()`'s own epilogue -- both are necessary before CS is
  handed to another device on the same bus. Skipping either leaves stale
  bytes in the RX FIFO for whatever reads next: confirmed on real hardware
  as the direct cause of an XPT2046 touch controller sharing the bus
  consistently reading back zeros instead of real ADC values, before this
  drain was added. If you fork `write_pixels()` for a different transfer
  mechanism, keep this epilogue.
- **XPT2046 shares a bus, never owns it**: `Xpt2046Touch::init()` does not
  call `spi_init()` -- `config.spi_instance` must already be brought up by
  whatever display driver owns the bus, and `read()` must not be called
  while that driver has an open `set_window()`/`write_pixels()` sequence.
- **DMA channels are always configurable, never hardcoded**: `Ili9486Config::
  dma_channel` (default `-1`, auto-claims any free channel) and
  `I2sAudioConfig::dma_channel` (default `0`, pico_audio_i2s claims that
  *specific* channel -- it has no "any free channel" mode) both exist so a
  consumer whose other drivers claim DMA channels by fixed number can keep
  this toolset's components off of them. Getting this wrong panics at
  startup ("DMA channel N is already claimed") -- confirmed on real
  hardware as a real regression (an unconfigurable auto-claim collided with
  Pico-PIO-USB's own hardcoded default channel) before `dma_channel` was
  added. Apply the same principle to any new component: no hardcoded pin,
  bus, clock, or resource-index value -- always a config field with a
  sensible default.
- **PSRAM bring-up**: `psram_init()` must run once from core0 before any
  allocation and before core1 starts. The RXDELAY clamp (max divisor) and
  `flash_devinfo_set_cs_size()` fix are baked in -- direct ports that skip
  them read back zeros.
- **USB HID**: the host stack owns a whole core (default core1). For DVI/HDMI
  builds where core1 is busy, set `run_on_core1 = false` and call
  `UsbHidHost::task()` from your core0 loop. Endpoint re-arming after an
  unplug is intentionally not retried (wedge-prevention finding).
- **SD card PIO/GPIO-base sharing**: if another PIO-based component (e.g.
  `usb_hid`'s Pico-PIO-USB, or a DVI/HDMI serializer) is also active, give
  `SdCardConfig::pio` a block none of them claim. If any configured SD pin
  is >= 32, set `gpio_base` to widen that PIO block's addressing window --
  skipping it doesn't error, it silently times out mounting (RP2350B wraps
  the out-of-range pin number back into 0-31 instead of rejecting it).
- **Reset buttons' watchdog tag is process-wide**: only one "pending tag"
  fits in the watchdog scratch registers at a time -- if you also use
  `watchdog_reboot()` directly elsewhere for something unrelated, that
  reboot won't carry a tag `consume_pending_watchdog_tag()` recognizes (it
  simply returns false), but make sure nothing else *also* writes
  `watchdog_hw->scratch[0]`/`[1]` for a different purpose, or the two will
  collide.
- **Screen**: `Screen` does not own widgets -- keep them alive for its
  lifetime. `BufferedDisplay` needs a caller-owned RGB565 framebuffer sized
  for whichever `DisplayPanel` it wraps -- e.g. 480x320x2 ≈ 300KB for an
  ILI9486 (allocate from PSRAM), or 320x240x2 = 150KB for a 320x240 ST7789
  panel (on RP2040's 264KB SRAM with no PSRAM, this is most of it; watch
  your total static+heap+stack budget).

## License

MIT -- see `LICENSE`. Derived drivers attribute their upstream origins in
each header; the third-party code the DVI/HDMI component carries forward
under its own, different license is called out below.

## Credits and third-party code

Every component here started from real-hardware-validated code in
consumer projects, which each component's own "Source lineage" note
(`AGENTS.md`) or header comment describes. The DVI/HDMI component in particular
vendors and adapts several external projects directly, each under its own
license (all compatible with, but distinct from, this toolset's own MIT
license above):

- **[Wren6991/PicoDVI](https://github.com/Wren6991/PicoDVI)** (Luke Wren,
  BSD-3-Clause) -- the base PIO-based DVI/TMDS serialiser + encoder
  (`components/dvi_hdmi/dvi.c`, `dvi_serialiser.*`, `tmds_encode.*`,
  `dvi_timing.*`, `common_dvi_pin_configs.h`, ...), vendored via Waveshare's
  RP2350-PiZero C example repository (`RP2350-PiZero/C/01-DVI/libdvi`).
- **[rh1tech/frank-hdmi-audio](https://github.com/rh1tech/frank-hdmi-audio)**
  (BSD-3-Clause, itself layered on Wren6991/PicoDVI) -- design basis for the
  HDMI data-island audio addition: the lock-free SPSC sample ring
  (`components/dvi_hdmi/audio_ring.{h,cpp}`, with one documented bug fixed
  relative to the original -- see that file's own header) and the C-linkage
  free-function shape the data-island packet encoder
  (`components/dvi_hdmi/data_packet.{h,cpp}`) follows. Its `docs/LLM_GUIDE.md`
  also documents the "half-pre-fill the audio ring at init" technique this
  component uses.
- **[shuichitakano/pico_lib](https://github.com/shuichitakano/pico_lib)**
  (Shuichi Takano, MIT, Copyright (c) 2021) -- the CEA-861 InfoFrame/ACR/
  audio-sample packet layouts and TERC4/BCH-parity encode algorithm
  (`dvi::DataPacket`), translated into `data_packet.{h,cpp}`'s C-linkage
  shape.

See `components/dvi_hdmi/README.md` for the full provenance writeup,
including exactly which files/blocks came from where and which patches are
this toolset's own (tagged `// PATCH (...)` in the vendored sources).

## Building the examples

```
cmake -B build -DPICO_BOARD=pico
cmake --build build
ls build/components/*/example/*.uf2 build/libs/*/example/*.uf2
```

Flash the matching `.uf2` for the board and check the serial console output
(`pico_enable_stdio_usb` on most examples).