# Sharing one SPI bus

Several supported boards put more than one device on one SPI instance:

| Board | Bus | Devices |
|---|---|---|
| Waveshare PiZero + LCD (A) | SPI1 | LCD (CS 8) + touch (CS 7) |
| CrowPanel PICO HMI 2.8" | SPI1 | LCD (CS 9) + touch (CS 16) + SD (CS 22) |

## Rules

1. **One owner calls `spi_init()`** -- the display driver. `Xpt2046Touch` and the
   SD driver (in native-SPI mode) never initialise the peripheral; pass them the
   display's bus: `cfg.spi_instance = lcd.spi();`.
2. **Never interleave transactions.** `set_window()` leaves the display's CS
   asserted until `end_write()`. Do not read touch or touch the SD card in
   between.
3. **Let DMA finish.** After a DMA pixel push call `finish_pixels_dma()` before
   `end_write()`: it drains the SPI RX FIFO and waits out BSY, exactly like the
   epilogue of `spi_write_blocking()`. Skipping either leaves stale bytes in the
   RX FIFO and the next reader (touch) gets zeros -- confirmed on real hardware.
4. **Set the baud rate before asserting CS.** `spi_set_baudrate()` disables the
   peripheral and glitches SCK/MOSI. Calling it after CS is low shifted display
   rows by about a pixel. The toolset drivers do it in the right order; keep that
   if you fork a transfer path.
5. **Each device wants its own clock, and only the display and touch drivers
   change it.** The display runs at its pixel clock, touch at 2 MHz, but
   `pico_fatfs` sets the SD clock only during card initialisation. On the
   CrowPanel, restore it after any display/touch access:
   `spi_set_baudrate(spi1, pico_fatfs_get_clk_fast_freq())` (PicoBoot does this,
   including for USB MSC reads).
6. **Reads from the LCD return zeros.** On the Waveshare panel MISO belongs to
   the touch controller; display register reads are a false negative, not a
   fault.
7. **Only one `SdCard` is mounted at a time** per program.

## Why the display is write-only

The Waveshare/SunFounder panels sit behind a shift-register bridge whose MISO
line is wired to the touch controller, so the controller cannot be read back at
all. The CrowPanel's `St7789Config` has no MISO field for the same reason: MISO
exists on the bus but belongs to touch and SD.

## Sources

`components/ili9486`, `components/st7789`, `components/xpt2046`,
`components/sdcard`; PicoBoot `targets/picoboot_lvgl_crowpanel/main.cpp`;
TOM6809 `README-PICO.md` (baud-before-CS bug).
