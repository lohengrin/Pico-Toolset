# sdcard

FatFs R0.15 over `elehobica/pico_fatfs`, using SD in SPI mode either on native
hardware SPI or bit-banged through a PIO state machine (the usual case on boards
that wire the socket for SDIO).

| | |
|---|---|
| Target / option | `pico_toolset_sdcard` / `PICO_TOOLSET_BUILD_SDCARD` (ON); sub-option `PICO_TOOLSET_SDCARD_STDIO` (OFF) |
| Dependency | `pico_fatfs`, fetched by `cmake/pico_fatfs.cmake` (FetchContent `main`) or `PICO_FATFS_DIR`; it defines `PICO_PIO_USE_GPIO_BASE=1` |
| Links | `pico_stdlib`, `pico_fatfs`, `hardware_pio`, `hardware_spi` |
| Example | `components/sdcard/example/sdcard_example.cpp` |
| Presets | `kWaveshareRp2350PiZero`, `kPicoDvCarrier`, `kElecrowCrowPanelPicoHmi28` |

## Config -- `SdCardConfig`

| Field | Default | Notes |
|---|---|---|
| `spi_instance` | -- | `nullptr` = PIO bit-bang; a real instance = native hardware SPI |
| `pin_miso`, `pin_cs`, `pin_sck`, `pin_mosi` | -- | **must set** |
| `pullup` | true | |
| `clk_slow_hz` | 100 kHz | identification phase |
| `clk_fast_hz` | 10 MHz | data phase; a higher clock over jumper wires causes intermittent corruption that reads as "no card" |
| `pio`, `sm` | -- , 0 | **set `pio`**; ignored when `spi_instance` is set |
| `gpio_base` | -1 | 16 when any pin >= 32 ([PIO window](../guides/pio-gpio-window.md)) |

## Presets

| Preset | Mode | Pins (SCK / MOSI / MISO / CS) | PIO |
|---|---|---|---|
| `kWaveshareRp2350PiZero` | PIO SPI | 30 / 31 / 40 / 43 | PIO1 SM0, `gpio_base` 16 (PIO0 is USB-PIO's) |
| `kPicoDvCarrier` | PIO SPI | 5 / 18 / 19 / 22 | PIO1 SM0 (PIO0 is I2S's) |
| `kElecrowCrowPanelPicoHmi28` | native `spi1`, shared with display + touch | 10 / 11 / 12 / 22 | none |

## API -- `SdCard`

- `bool init(cfg)`; `is_mounted()`, `last_mount_result()` (raw `FRESULT`),
  `used_native_spi()`.
- `list_files(const std::vector<std::string>& extensions)`;
  `struct FileInfo { name, size, is_dir }`;
  `list_file_info(exts, max_entries = 0, bool* truncated = nullptr, skip_hidden = true)`;
  `bool list_dir(path, exts, std::vector<FileInfo>& out, max_entries = 0,
  truncated = nullptr, skip_hidden = true)` (one level, folders always included).
- `read_file(name)` (`std::vector<uint8_t>`), `read_file_pmr(name, resource)`.

## POSIX/stdio shim (`PICO_TOOLSET_SDCARD_STDIO=ON`)

Adds `sdcard_stdio.cpp`, which overrides newlib's weak syscalls (`open`, `read`,
`write`, `lseek`, `fstat`, `stat`, `isatty`) so `fopen`/`fread` work over FatFs.
It is a whole-program decision, hence opt-in. PicoDoom uses it so DOOM's WAD loader
runs unmodified. The shim also maps errno and permission bits (storage-robustness
work alongside `usb_composite`).

## Gotchas

- `FR_DISK_ERR`/`FR_NOT_READY` (1/3) = wiring or clock; `FR_NO_FILESYSTEM` (13) =
  not FAT.
- Pins >= 32 without `gpio_base` produce a mount that times out forever.
- Only one `SdCard` mounted at a time.
- On a shared bus (CrowPanel) restore the SD clock after display/touch access
  ([guide](../guides/shared-spi-bus.md)).
- PicoBoot regenerates `ffconf.h` with `FF_LFN_UNICODE=2`/`FF_CODE_PAGE=850`,
  saving about 55 KB.

Lineage: unifies two board-specific SD-in-SPI variants (Pico DV and Waveshare)
into one config-driven driver.
