// TinyUSB configuration for the Pico-Toolset PIO-USB HID host.
//
// rhport 0 = the native Pico USB controller (kept in DEVICE role so the board
// can also be a USB device, e.g. a serial console); rhport 1 = the PIO-USB
// root hub in HOST role. This is the standard Pico-PIO-USB host layout.
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// RHPort number used for host can be defined by board.mk, default to port 0.
#ifndef BOARD_TUH_RHPORT
#define BOARD_TUH_RHPORT 1
#endif

// Pico-PIO-USB requires the host stack on a dedicated root-hub port.
#ifndef CFG_TUH_RPI_PIO_USB
#define CFG_TUH_RPI_PIO_USB 1
#endif

#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif
#define CFG_TUSB_DEBUG 0

#ifndef CFG_TUSB_MCU
// RP2350 uses the same TinyUSB MCU port as RP2040 (no OPT_MCU_RP2350 in the
// SDK-vendored TinyUSB), so this is correct for both chips.
#define CFG_TUSB_MCU OPT_MCU_RP2040
#endif

#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE)
#endif

#ifndef CFG_TUSB_RHPORT1_MODE
#define CFG_TUSB_RHPORT1_MODE (OPT_MODE_HOST)
#endif

// Host-side tuning (propagated from UsbHidConfig by the host's application).
#ifndef CFG_TUH_ENABLED
#define CFG_TUH_ENABLED 1
#endif
#ifndef CFG_TUH_DEVICE_MAX
#define CFG_TUH_DEVICE_MAX 4
#endif
#ifndef CFG_TUH_HUB
#define CFG_TUH_HUB 0
#endif
#ifndef CFG_TUH_HID
#define CFG_TUH_HID 4
#endif

// Buffer size used by the host driver (largest HID report we care about).
#ifndef CFG_TUH_HID_EPIN_BUFSIZE
#define CFG_TUH_HID_EPIN_BUFSIZE 64
#endif
#ifndef CFG_TUH_HID_EPOUT_BUFSIZE
#define CFG_TUH_HID_EPOUT_BUFSIZE 64
#endif

// Device-side (rhport 0): CDC serial console (used with pico_stdio_usb), so
// the board can print diagnostics over USB while hosting on the PIO port.
#ifndef CFG_TUD_ENABLED
#define CFG_TUD_ENABLED 1
#endif
#ifndef CFG_TUD_CDC
#define CFG_TUD_CDC 1
#endif
#ifndef CFG_TUD_CDC_RX_BUFSIZE
#define CFG_TUD_CDC_RX_BUFSIZE 256
#endif
#ifndef CFG_TUD_CDC_TX_BUFSIZE
#define CFG_TUD_CDC_TX_BUFSIZE 256
#endif

#ifdef __cplusplus
}
#endif

#endif // TUSB_CONFIG_H_