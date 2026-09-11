// TinyUSB configuration for the Pico-Toolset PIO-USB HID host.
//
// rhport 0 = the native Pico USB controller (kept in DEVICE role so the board
// can also be a USB device, e.g. a serial console); rhport 1 = the PIO-USB
// root hub in HOST role. This is the standard Pico-PIO-USB host layout.
//
// DANGER -- must be the ONLY tusb_config.h reachable anywhere in the final
// build, not just on pico_toolset_usb_hid's own include path: pico-sdk's
// tinyusb_host is an INTERFACE library, so its core sources (usbh.c, hub.c,
// hcd_pio_usb.c, tusb.c...) get compiled fresh into *every* target that
// links it, including transitively into the final executable itself (not
// just into this component's own .a archive) -- and the linker silently
// prefers whichever copy was compiled directly into the executable over the
// one inside a linked static library, with no duplicate-symbol error either
// way. If the final executable also supplies its own, different
// tusb_config.h (e.g. a leftover from before adopting this component), that
// copy wins for TinyUSB's actual core implementation while this component's
// own .cpp sources stay compiled against this file -- a real ABI mismatch
// (CFG_TUH_HUB, CFG_TUH_DEVICE_MAX, and friends all affect internal struct
// layout) that manifests as USB devices silently not working at all, not a
// build error. Confirmed on real hardware as the direct cause of a
// keyboard+mouse combo dongle failing completely after this exact mistake.
// If your project has its own tusb_config.h, delete it and rely on this one
// instead (override individual macros via target_compile_definitions if you
// need different values, not a competing header).
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
// Hub support ON by default: the common real-world topology for a PIO-USB
// host is a keyboard+mouse combo dongle or a physical hub, both of which
// enumerate as a hub with devices behind it -- with this off, nothing behind
// a hub is ever seen at all (TOM6809 real-hardware finding: this was the
// root cause of a keyboard+mouse combo failing completely, not a narrow
// single-device edge case).
#ifndef CFG_TUH_HUB
#define CFG_TUH_HUB 1
#endif
#ifndef CFG_TUH_ENUMERATION_BUFSIZE
#define CFG_TUH_ENUMERATION_BUFSIZE 256
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