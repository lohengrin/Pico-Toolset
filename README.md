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
| PSRAM     | `pico_toolset_psram`    | RP2350-only external PSRAM bring-up (QMI CS1), self-test, free-list allocator, `std::pmr` adapter. No-op on RP2040. |
| USB HID   | `pico_toolset_usb_hid`  | PIO-USB TinyUSB host: keyboard/mouse/HID-gamepad + XInput, double-buffered cross-core state, unified `GamepadState`. |

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
│   ├── psram/     include/pico_toolset/psram.h     src/  example/
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
| `PICO_TOOLSET_BUILD_PSRAM`   | ON | Build PSRAM driver + example (RP2350 only) |
| `PICO_TOOLSET_BUILD_USB_HID` | ON | Build PIO-USB HID host + example |
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
- **PSRAM bring-up**: `psram_init()` must run once from core0 before any
  allocation and before core1 starts. The RXDELAY clamp (max divisor) and
  `flash_devinfo_set_cs_size()` fix are baked in -- direct ports that skip
  them read back zeros.
- **USB HID**: the host stack owns a whole core (default core1). For DVI/HDMI
  builds where core1 is busy, set `run_on_core1 = false` and call
  `UsbHidHost::task()` from your core0 loop. Endpoint re-arming after an
  unplug is intentionally not retried (wedge-prevention finding from
  TOM6809).
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