# Clocks and power

The toolset's presets deliberately carry only conservative defaults. The
settings below are what the client projects (PicoDoom, TOM6809, PicoBoot) run on
real hardware; none of it is set by the toolset itself. Apply it in your own
`main()` **before** initialising the drivers.

> Numbers marked *validated* were confirmed on real hardware by the named
> project. The RP2350 and RP2040 operate far outside their datasheet ratings in
> several rows; treat them as recipes that worked on specific boards, not as
> guarantees.

## Summary

| Build | `clk_sys` | VREG | Why |
|---|---|---|---|
| RP2350 PiZero, HDMI | **252 MHz** exactly | 1.20 V | libdvi derives the TMDS bit clock (252 MHz for 640x480p60) from `clk_sys`; 200 MHz gave no signal |
| RP2350 PiZero, LCD / USB host | **264 MHz** | 1.20 V | multiple of 12 MHz (Pico-PIO-USB); lets `clk_peri` reach 132 MHz for the ST7796 |
| RP2040 + Pico DV, HDMI | **252 MHz** | 1.20 V | same TMDS requirement; validated by PicoBoot |
| Neither USB-PIO nor HDMI | 200 MHz is fine | 1.20 V if above stock | |
| Stock | 125 MHz (RP2040) / 150 MHz (RP2350) | default | |

## Order of operations (RP2350 PiZero)

1. **Raise the flash QMI divider first.** Write QMI M0 `CLKDIV` to 2 *before*
   raising `clk_sys`; otherwise XIP instruction fetch can misread. Flash SPI then
   runs at 126 MHz (HDMI) or 132 MHz (LCD) -- above the ~75 MHz proven at
   boot2's divisor with a 150 MHz `clk_sys`. If XIP misreads appear, raise the
   divisor.
2. **Raise VREG to 1.20 V** (`vreg_set_voltage(VREG_VOLTAGE_1_20)`) and wait
   about 10 ms.
3. **Set `clk_sys`** (`set_sys_clock_khz(..., required)`).
4. **Re-source `clk_peri`** if you need SPI faster than 24 MHz (below).
5. **Then** call `psram_init()` -- it calibrates the QMI timing against the
   *current* `clk_sys`.
6. **Then** `dvi_init()` (it reads `clk_sys` to derive the bit clock).

A combined HDMI + USB build must check HDMI first: the earlier ordering ran
264 MHz and hung during the PSRAM self-test. PicoBoot performs the DVI clock
setup only after its fast-boot check, so a booted app starts from the pristine
power-on clock.

## `clk_peri` and SPI speed

`clk_peri` stays on the 48 MHz USB PLL unless you move it, and the SPI divider
only supports **even** divisors, so SPI is capped at 24 MHz. Tie it to
`clk_sys`:

```cpp
clock_configure_undivided(clk_peri, 0,
    CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, clock_get_hz(clk_sys));
```

(or build with `PICO_CLOCK_ADJUST_PERI_CLOCK_WITH_SYS_CLOCK`). Consequences with
`clk_sys` = 264 MHz:

- `spi_set_baudrate()` rounds the achieved rate *down*, never up. 33.0 MHz =
  264/8 is reachable; a 33.33 MHz request lands on 33.0 MHz; 44 MHz (264/6) is
  not reachable by asking for 40.
- ST7796U: 132 MHz = 264/2 (the fastest SPI can go). Validated.
- ILI9486: 33 MHz clean, 40 MHz and above corrupt. Validated.
- On stock RP2350 (`clk_peri` 150 MHz) a 24 MHz request gives 18.75 MHz and 25
  MHz gives 25 MHz.

## PSRAM clock

The `kWaveshareRp2350PiZero` PSRAM preset requests 30 MHz. PicoDoom and PicoBoot
run **126 MHz** (HDMI) and **132 MHz** (LCD), QMI divisor 2, with VREG 1.20 V --
above the chip's ~100 MHz rating and stable in practice. At 200 MHz `clk_sys` the
divisor-2 point is exactly 100 MHz. The `psram_qspi_sweep_example` finds the
maximum stable rate for a given board. A QMI RXDELAY divisor overflow in an
earlier `psram.cpp` had to be fixed for 1.20 V operation to be stable.

## USB-PIO needs a multiple of 12 MHz

Pico-PIO-USB's bit timing requires `clk_sys` to be an exact multiple of 12 MHz
(`usb_tx.pio`). 252 and 264 MHz qualify; 150 and 200 MHz do not (symptom: erratic
or hanging USB under load).

## Verification checklist

- [ ] `clock_get_hz(clk_sys)` prints the expected value at boot
- [ ] `PsramStatus::clock_hz` matches what you asked for (it is the *achieved*
      QMI rate)
- [ ] `Ili9486::pixel_clock_actual_hz()` / `St7796::pixel_clock_actual_hz()`
      report the rate you intended (after the first `set_window()`)
- [ ] `PsramStatus::test_ok` is true after a cold boot **and** a soft reset

## Sources

PicoDoom `src/PicoDoom.cpp`, `src/board_config.hpp`, `docs/HDMI_PLAN.md`;
TOM6809 `README-PICO.md`, `src/ui_pico/main_pico.cpp`; PicoBoot `README.md`,
`targets/picoboot_lvgl_dvi/main.cpp`; Wren6991/PicoDVI README (RP2040 designed
for 133 MHz, 720p30 needs overvoltage).
