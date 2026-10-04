# psram

RP2350-only external QSPI PSRAM bring-up (QMI chip select 1): detection, a
sampled read/write self-test, a first-fit allocator and a `std::pmr` adapter.

| | |
|---|---|
| Target / option | `pico_toolset_psram` / `PICO_TOOLSET_BUILD_PSRAM` (ON) |
| Availability | **RP2350 only.** `components/psram/CMakeLists.txt` builds it under `if(PICO_RP2350)`; on RP2040 nothing is built and the header is empty (`#if PICO_RP2350`), so there is no no-op API |
| Links | public `pico_stdlib`, `hardware_psram`; private `hardware_flash`, `hardware_clocks`, `pico_flash`, `pico_multicore` |
| Examples | `psram_example` (detect, self-test, allocator, pmr vector); `psram_qspi_sweep_example` (find the maximum stable QMI clock; built `copy_to_ram`, links `hardware_vreg`, `hardware_xip_cache`) |
| Presets | `configs::psram::kWaveshareRp2350PiZero` |
| Board | [Waveshare RP2350-PiZero](../boards/waveshare-rp2350-pizero.md) |

## Config -- `PsramConfig`

| Field | Default | Notes |
|---|---|---|
| `cs_pin` | -- | **must set**; QSPI chip select (GPIO47 on the PiZero) |
| `max_clock_hz` | 30 000 000 | 0 = SDK default; the QMI divisor is an integer and rounds the rate *down* |
| `run_self_test` | true | read/write pattern test |
| `self_test_samples` | 64 | |

Preset: `cs_pin` 47, 30 MHz, self-test on, 64 samples.

## API

- `PsramStatus psram_init(const PsramConfig&)` -- call **exactly once**, from core0,
  before any allocation and before launching core1 work.
- `const PsramStatus& psram_status()`.
- `bool psram_set_clock_hz(uint32_t)` -- retune the QMI CS1 clock without
  re-detecting; same flash-unreadable hazard as `psram_init()`.
- `extern "C" void* psram_malloc(size_t)` (first-fit, 8-byte aligned, null on
  OOM/size 0/no PSRAM), `extern "C" void psram_free(void*)`,
  `size_t psram_used_bytes()`.
- `class PsramResource : std::pmr::memory_resource` -- traps with `bkpt #0` on
  exhaustion (the toolchain builds with `-fno-exceptions`).

`PsramStatus`: `present`, `test_ok`, `size_bytes`, `self_test_samples`,
`fail_offset`, `fail_expected`, `fail_actual`, `fail_on_second_pattern`,
`clk_sys_hz_at_test`, `clock_hz` (the *achieved* QMI rate).

`present = false` is a normal outcome when the chip is not fitted (the PiZero
ships with the footprint unpopulated), so one firmware image can cover boards with
and without PSRAM.

## Constraints

- **Flash safety.** `psram_detect_cs_and_size()`/`psram_reinitialize()` briefly make
  flash unreadable via XIP. `psram_init()` runs them under `flash_safe_execute()`
  when the other core is lockout-ready, else with interrupts disabled -- sufficient
  only if the other core is not running yet. `usb_hid` deliberately does *not*
  register core1 as a lockout victim (the lockout IRQ steals words from the raw
  inter-core FIFO). Initialise PSRAM first.
- **Clock order.** Call after any `clk_sys` change; it calibrates against the
  current clock ([clocks](../guides/clocks-and-power.md)).
- **High clocks.** The chip is rated about 100 MHz. PicoDoom and PicoBoot run
  126 MHz (HDMI) / 132 MHz (LCD), divisor 2, VREG 1.20 V, stable in practice; use
  the sweep example on your board, boot-test cold and soft reset, and watch for
  runtime corruption. The preset stays at 30 MHz.
- **No cross-core coherency.** PSRAM sits behind the XIP cache; core1 should read
  it only through DMA, and a DVI framebuffer belongs in SRAM.
- The RXDELAY clamp and `flash_devinfo_set_cs_size()` fix are baked in.
