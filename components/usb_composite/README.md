# USB composite (`pico_toolset_usb_composite`)

TinyUSB **device** stack on the native USB port (rhport 0): one device with

- a **CDC** serial port -- optionally installed as the pico-sdk stdio driver
  (`printf`/`getchar`), replacing `pico_stdio_usb`;
- an **MSC** drive backed by any block device you supply (SD card, RAM disk...)
  through four function pointers (`UsbBlockDevice`: ready, block count,
  read, write -- 512-byte blocks). It also acknowledges `SYNCHRONIZE CACHE`,
  which hosts send after writes (rejecting it makes them remount read-only);
- pico-sdk's **vendor reset interface**, so `picotool reboot -f -u`,
  `picotool load -f ...` and the 1200-baud CDC touch work without pressing
  BOOTSEL.

```cpp
pico_toolset::UsbCompositeConfig cfg;
cfg.block_device = {ctx, ready, block_count, read, write};
pico_toolset::usb_composite_init(cfg);
while (true) { pico_toolset::usb_composite_task(); /* or just getchar() */ }
```
Call `usb_composite_task()` often (the stdio input path pumps it while it
waits). Host enumeration stalls if it is starved -- pump it around slow
initialisation and redraws.

Default PID is `0x000A` (the pico-sdk CDC PID) so picotool's stock udev rules
cover it; override `vid`/`pid` for a product.

## Two variants: device-only and `_hid`

TinyUSB allows exactly one `tusb_config.h` per build, and it must be the one
the *final executable's* TinyUSB core is compiled with (see the DANGER note in
`usb_hid/tusb_config.h`). So the sources are built twice:

| Target | Config | Use when |
|---|---|---|
| `pico_toolset_usb_composite` | `config/tusb_config.h` (device only) | no PIO-USB host |
| `pico_toolset_usb_composite_hid` | `usb_hid`'s `tusb_config.h` | firmware is also a PIO-USB host (keyboard/mouse) |

The `_hid` variant exists only if `pico_toolset_usb_hid` was added *before*
this component. Link **exactly one** variant into an executable. The
interface order is CDC (0,1), reset (2), MSC (3); interface 2 matches
`usb_hid`'s `PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF`.

Not compatible with `pico_stdio_usb` (it brings its own descriptors).
Reference consumer: PicoBoot.
