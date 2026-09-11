# Pico-Toolset

Reusable, configuration-driven drivers for Raspberry Pi Pico (RP2040 and
RP2350), extracted from the TOM6809, PicoDoom and PiCoMonitor projects and
re-packaged as a set of independent CMake components. Every component is
pin/config-structured so it adapts to any board wiring, ships with an example
that doubles as an integration smoke-test, and can be built or skipped
independently.

## Components

| Component | Target | Description |
|-----------|--------|-------------|
| SSD1306   | `pico_toolset_ssd1306`  | I2C monochrome OLED driver (128x64/128x32/...) with framebuffer, BMP blitting, basic primitives. |
| ILI9486   | `pico_toolset_ili9486`  | 480x320 SPI TFT driver for Waveshare-style boards where the panel sits behind a 16-bit shift register; DMA-backed pixel streaming, backlight PWM. |
| XPT2046   | `pico_toolset_xpt2046`  | Resistive touch controller sharing an SPI bus with a display driver (e.g. ILI9486); raw ADC reads + a linear calibration helper. |
| PSRAM     | `pico_toolset_psram`    | RP2350-only external PSRAM bring-up (QMI CS1), self-test, free-list allocator, `std::pmr` adapter. No-op on RP2040. |
| USB HID   | `pico_toolset_usb_hid`  | PIO-USB TinyUSB host: keyboard/mouse/HID-gamepad + XInput + DualSense (VID/PID-detected), double-buffered cross-core state, unified `GamepadState`. |
| I2S audio | `pico_toolset_i2s_audio` | Float-sample I2S DAC output (e.g. PCM5100A) via pico-extras' `pico_audio_i2s`; non-blocking queue, config-driven pins/DMA channel/PIO SM. OFF by default -- needs pico-extras set up by the consumer (see below). |
| SD card   | `pico_toolset_sdcard`   | FatFs R0.15 (elehobica/pico_fatfs) over native or PIO-bit-banged SPI; config-driven pins/PIO/gpio_base, `list_files()`/`read_file()`/`read_file_pmr()`. |
| Reset buttons | `pico_toolset_reset_buttons` | N debounced, active-HIGH momentary buttons + a generic tagged-watchdog-reboot pair (`watchdog_reboot_with_tag()`/`consume_pending_watchdog_tag()`), reusable for any "boot straight into mode X" use case. |

## Libraries

Header-only helpers that sit above the components rather than being
standalone drivers.

| Library | Target | Description |
|---------|--------|-------------|
| Screen   | `pico_toolset_screen`  | Pluggable `DisplayDriver` abstraction + PiCoMonitor-style slot widget composition, with SSD1306/ILI9486/Pimoroni adaptors. |

All code lives in namespace `pico_toolset` and targets C++20.

## Layout

```
pico-toolset/
├── CMakeLists.txt            # top-level: options + subdirectories
├── cmake/
│   ├── pico-toolset.cmake    # helper for FetchContent consumers
│   └── pico_pio_usb.cmake    # makes Pico-PIO-USB available (submodule or fetch)
├── components/
│   ├── ssd1306/   include/pico_toolset/ssd1306.h   src/  example/
│   ├── ili9486/   include/pico_toolset/ili9486.h   src/  example/
│   ├── xpt2046/   include/pico_toolset/*.h         src/  example/
│   ├── psram/     include/pico_toolset/psram.h     src/  example/
│   ├── i2s_audio/ include/pico_toolset/i2s_audio.h src/  example/
│   ├── sdcard/    include/pico_toolset/sdcard.h    src/  example/
│   ├── reset_buttons/ include/pico_toolset/reset_buttons.h src/ example/
│   └── usb_hid/   include/pico_toolset/*.h  src/  example/  tusb_config.h
└── libs/
    └── screen/    include/pico_toolset/*.h  example/
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
| `PICO_TOOLSET_BUILD_XPT2046` | ON | Build XPT2046 touch driver + example |
| `PICO_TOOLSET_BUILD_PSRAM`   | ON | Build PSRAM driver + example (RP2350 only) |
| `PICO_TOOLSET_BUILD_USB_HID` | ON | Build PIO-USB HID host + example |
| `PICO_TOOLSET_BUILD_I2S_AUDIO` | OFF | Build I2S audio output + example (needs pico-extras, see below) |
| `PICO_TOOLSET_BUILD_SDCARD`  | ON | Build SD card (FatFs/pico_fatfs) driver + example |
| `PICO_TOOLSET_BUILD_RESET_BUTTONS` | ON | Build debounced-buttons + tagged-watchdog-reboot helper + example |
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

## Minimal usage

### SSD1306

```cpp
pico_toolset::Ssd1306Config cfg;        // defaults: i2c1, SDA19/SCL18, 400kHz, 0x3C
pico_toolset::Ssd1306 oled;
oled.init(cfg);
oled.clear();
oled.draw_string(0, 0, 1, "Hello");
oled.show();
```

### ILI9486

```cpp
pico_toolset::Ili9486Config cfg;        // defaults: spi1, SCK10/MOSI11/CS8/DC24/RST25
pico_toolset::Ili9486 lcd;
lcd.init(cfg);
lcd.fill_solid(0x001F);                 // solid blue
lcd.set_window(0, 0, 100, 100);
lcd.write_pixels(span_of_100x100_pixels);
lcd.end_write();
```

### XPT2046 touch (shares a bus with a display driver)

```cpp
pico_toolset::Ili9486Config lcd_cfg;    // spi_init()s spi1 -- must happen first
pico_toolset::Ili9486 lcd;
lcd.init(lcd_cfg);

pico_toolset::Xpt2046Config touch_cfg;  // defaults: spi1, CS7/IRQ17, 2MHz
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
auto st = pico_toolset::psram_init({}); // defaults: CS=GPIO47, self-test on
void* p = pico_toolset::psram_malloc(4096);
pico_toolset::PsramResource res;        // std::pmr::memory_resource
std::pmr::vector<uint8_t> big(&res);
```

### USB HID host

```cpp
pico_toolset::UsbHidConfig cfg;         // defaults: D+ = GPIO28, core1 dedicated
pico_toolset::UsbHidHost usb;
usb.init(cfg);
while (true) {
    for (char ch; (ch = usb.consume_typed_ascii_char()) != 0;) putchar(ch);
    auto g = usb.gamepad_state(0);
    auto m = usb.mouse_state();
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

### I2S audio (pico-extras -- see the setup section above)

```cpp
pico_toolset::I2sAudioConfig cfg;       // defaults: 44.1kHz mono, DATA26/BCK27, DMA ch.0, PIO SM0
pico_toolset::I2sAudioOutput audio;
if (!audio.init(cfg)) { /* handle failure */ }
std::array<float, 882> frame;           // [-1, 1] samples, e.g. one 20ms frame at 44.1kHz
// ...fill frame...
audio.queue_samples(frame);             // non-blocking; drops this call's audio if no buffer is free
```

### SD card

```cpp
pico_toolset::SdCardConfig cfg;         // defaults: PIO-bit-banged SPI, pio1/sm0
cfg.pin_miso = 19; cfg.pin_cs = 22; cfg.pin_sck = 5; cfg.pin_mosi = 18;
// cfg.gpio_base = 16;  // only if a configured pin is >= 32, see the doc comment
pico_toolset::SdCard sd;
if (!sd.init(cfg)) { /* no card / mount failed -- sd.last_mount_result() has the FRESULT */ }
for (const auto& name : sd.list_files({"txt", "bin"})) { /* ... */ }
std::vector<uint8_t> data = sd.read_file("config.bin");
```

`pico_fatfs` is fetched via `FetchContent` by default; set `PICO_FATFS_DIR`
to point at a local checkout instead (same pattern as `PICO_PIO_USB_DIR`
above).

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
  unplug is intentionally not retried (wedge-prevention finding from
  TOM6809).
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
  lifetime. `Ili9486Driver` needs a caller-owned RGB565 framebuffer (480x320x2
  ≈ 300KB; allocate from PSRAM).

## License

MIT -- see `LICENSE`. Derived drivers attribute their upstream origins in
each header.

## Building the examples

```
cmake -B build -DPICO_BOARD=pico
cmake --build build
ls build/components/*/example/*.uf2 build/libs/*/example/*.uf2
```

Flash the matching `.uf2` for the board and check the serial console output
(`pico_enable_stdio_usb` on most examples).