# USB-PIO host and TinyUSB pitfalls

Applies to `usb_hid` (a USB host over Pico-PIO-USB) and `usb_composite` (a USB
device exposing CDC + mass storage). See their component pages for the API.

## Two TinyUSB roles in one binary

With `usb_hid` the program runs **two** TinyUSB controllers: rhport 0 is the
native USB device (the CDC console, picotool reset) and rhport 1 is the PIO-USB
host. This requires a single project-owned `tusb_config.h` -- the one in
`components/usb_hid/`. It must be the *only* `tusb_config.h` in the final build;
a second one makes the TinyUSB core and the component disagree on struct layouts
and "USB devices silently don't work at all".

## Linking `tinyusb_host` silently disables the SDK's USB defaults

Linking `tinyusb_host` defines `LIB_TINYUSB_HOST`, which flips **seven** SDK
macros to 0 across two headers: three in `pico_stdio_usb`'s `stdio_usb.h`
(own descriptors, `tusb_init`, IRQ background task) and four in
`pico_usb_reset/usb_reset_config.h` (vendor reset interface, MS OS 2.0
descriptor, baud-rate reset). Only one failure is loud (a link error); the rest
give a dead CDC console or no picotool reset at run time. `pico_toolset_usb_hid`
forces all seven back on as `PUBLIC` compile definitions so consumers inherit a
working console. If you write your own host integration, grep both headers for
`LIB_TINYUSB_HOST`. The first flash after fixing it still needs BOOTSEL.

`-Wall -Wextra` also leaks from the `tinyusb_host` target onto the whole
executable.

## Core and IRQ placement

- The host stack owns a whole core by default (`run_on_core1 = true`). Hotplug
  after boot only works with a dedicated core.
- The default core1 stack is 4 KB from SCRATCH_X; an 8 KB request failed to
  link. TOM6809 uses `multicore_launch_core1_with_stack()` with 16 KB.
- With `run_on_core1 = false`, `UsbHidHost::init()` and `task()` must run on the
  **same** core (Pico-PIO-USB's SOF-timer IRQ binds to the core that registered
  it).
- libdvi's per-scanline DMA IRQ and Pico-PIO-USB's SOF-timer IRQ cannot share a
  core (no picture at all on the PiZero). HDMI builds run DVI on core1 and poll
  USB from core0 with `kWaveshareRp2350PiZeroHdmi`.
- On RP2040 the DVI DMA IRQ can starve the SIO FIFO IRQ; see the Pico DV board
  doc for the priority fix.
- Do not make a component register `flash_safe_execute_core_init()` on core1
  automatically: the lockout IRQ steals words from the raw inter-core FIFO and
  broke a consumer's blit hand-off on real hardware.

## Behaviour notes

- `clk_sys` must be a multiple of 12 MHz ([clocks guide](clocks-and-power.md)).
- A double-buffered keyboard state publishes only on change (`memcmp`); this was
  found on real hardware.
- Endpoint re-arming after an unplug is intentionally not retried (it wedged the
  host stack).
- Mouse boot-protocol reports are 3 bytes; the generic gamepad layout is an
  unverified fallback; XInput and DualSense are VID/PID-detected.
- **Audio over USB is not possible** through PIO-USB: TinyUSB has no host UAC
  driver and `pio_usb_host.c` has no isochronous transfers.
- A hard-fault handler cannot print from fault context: pumping `tud_task()`
  there re-enters TinyUSB and hangs. `fault_handler` therefore stashes PC/LR/CFSR
  and reports on the next boot.
- `usb_composite` (CDC + MSC device) is not compatible with `pico_stdio_usb`; it
  must be added after `usb_hid`, and exactly one of `pico_toolset_usb_composite`
  / `pico_toolset_usb_composite_hid` may be linked.

## Sources

`components/usb_hid/tusb_config.h`, `components/usb_hid/CMakeLists.txt`,
`components/usb_composite/README.md`; TOM6809 `README-PICO.md` ("LIB_TINYUSB_HOST
trap"); PicoDoom `docs/PLAN.md`, `docs/HDMI_PLAN.md`.
