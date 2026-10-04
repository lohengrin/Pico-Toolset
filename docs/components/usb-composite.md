# usb_composite

USB **device** exposing a CDC serial port and a mass-storage (MSC) interface over
one cable, backed by a block device you provide (an SD card, a RAM disk...).
Reference consumer: PicoBoot. The component's own
[`README.md`](../../components/usb_composite/README.md) has the full walkthrough;
this page is the summary.

| | |
|---|---|
| Option | `PICO_TOOLSET_BUILD_USB_COMPOSITE` (ON); must be added **after** `usb_hid` |
| Targets | `pico_toolset_usb_composite` (device only, own `config/tusb_config.h`) and `pico_toolset_usb_composite_hid` (exists only when `pico_toolset_usb_hid` exists; built against usb_hid's `tusb_config.h`, links `pico_toolset_usb_hid`). **Link exactly one.** |
| Links | `pico_stdlib`, `pico_unique_id`, `tinyusb_device`, `pico_usb_reset` |
| Example | `usb_composite_example` (RAM disk; stdio USB and UART disabled) |
| Key definitions | `CFG_TUD_CDC=1`, `CFG_TUD_MSC=1`, `CFG_TUD_MSC_EP_BUFSIZE=4096`, `CFG_TUD_CDC_TX_BUFSIZE=512`, `PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF=2` |

## API

`UsbBlockDevice { ctx, ready, block_count, read, write, sync, writable }`
(512-byte blocks; null `sync`/`writable` mean durable/writable).

`UsbCompositeConfig`: `block_device`; `manufacturer` "Pico-Toolset"; `product`
"Pico composite"; `msc_vendor` "Pico" (<= 8 chars); `msc_product` "Storage"
(<= 16 chars); `vid` 0x2E8A; `pid` 0x000A; `install_stdio` true.

Functions: `usb_composite_init(cfg)`, `usb_composite_task()`,
`usb_composite_media_changed()`, `bool usb_composite_ejected()`,
`bool usb_composite_cdc_connected()`.

## Gotchas

- Not compatible with `pico_stdio_usb`; interface order is CDC (0, 1), reset (2),
  MSC (3).
- Host enumeration stalls if `usb_composite_task()` is starved.
- Exactly one `tusb_config.h` per build ([USB pitfalls](../guides/usb-pio-pitfalls.md)).
- `sync`, `writable`, `usb_composite_media_changed()`, `usb_composite_ejected()`
  and `usb_composite_cdc_connected()` exist for storage robustness (eject, sync,
  write-protect, media change); a `UsbBlockDevice` over an SD card on a shared bus
  must keep the SD clock restored ([shared SPI](../guides/shared-spi-bus.md)).
