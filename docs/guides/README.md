# Guides

Cross-cutting hardware constraints that affect several boards and components.
Each guide states what was observed, on which board, and which project
confirmed it.

| Guide | Read it when |
|---|---|
| [Clocks and power](clocks-and-power.md) | you change `clk_sys`, run DVI/HDMI, USB-PIO, a fast SPI LCD or fast PSRAM |
| [PIO GPIO window](pio-gpio-window.md) | a PIO peripheral uses a pin number of 32 or above (RP2350B: SD on the PiZero, TMDS pins) |
| [Shared SPI bus](shared-spi-bus.md) | a display, a touch controller and/or an SD card sit on one SPI bus |
| [USB-PIO host and TinyUSB pitfalls](usb-pio-pitfalls.md) | you use `usb_hid` or `usb_composite`, or link `tinyusb_host` |
