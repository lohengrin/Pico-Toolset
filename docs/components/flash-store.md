# flash_store

Power-fail-safe settings store in the last flash sectors: appends CRC-checked
256-byte records and loads the newest valid one.

| | |
|---|---|
| Target / option | `pico_toolset_flash_store` / `PICO_TOOLSET_BUILD_FLASH_STORE` (ON) |
| Links | public `pico_stdlib`, `pico_flash`, `pico_multicore`, `hardware_flash` |
| Example | `components/flash_store/example/flash_store_example.cpp` |
| Presets | none (no board wiring) |
| Reference consumer | PiCoMonitor (settings in the last 2 sectors) |

## API

- `FlashStoreConfig { uint32_t offset; uint8_t sector_count; }` with
  `static constexpr at_end_of_flash(flash_size_bytes, sectors = 2)`.
- `FlashIo { read, erase_sector, program_page }` -- function pointers; RAM-backed
  implementations make host tests possible.
- `FlashStore`: `kSectorSize` 4096, `kPageSize` 256, `kMaxPayload` 240;
  `bool init(cfg, io)`, `bool init(cfg)` (real flash); `has_record()`,
  `sequence()`; `bool load(std::span<uint8_t> out, size_t& len)`;
  `bool save(std::span<const uint8_t>)` (appends; no-op if identical to the newest).

## Constraints

- **Reserve the range in your linker script**; the library does not.
- **Keep `PICO_FLASH_SIZE_BYTES` the real flash size** and shorten only the linker
  region: the SDK `hard_assert`s every `flash_range_erase`/`flash_range_program`
  against it (PiCoMonitor's settings area is the last two sectors of the 2 MiB
  CrowPanel/Pico/Pico W flash).
- Erase/program run through `flash_safe_execute()` when core1 is a registered
  lockout victim, else with interrupts disabled. Do not use with *unregistered*
  code running on core1.
- RP2350 under PicoBoot's flash address translation: reads go through XIP at the
  physical offset and would not see the physical tail, and flash programming APIs
  are not translated -- an app writing near the start would corrupt the
  bootloader.
- The page buffer is a member, so `save()` does not put a 256-byte page on the
  stack. PiCoMonitor still raised its core0 stack to 4 KiB
  (`PICO_STACK_SIZE=0x1000`) because the SDK's 2 KiB default overflowed once its
  flash-save path was added.
