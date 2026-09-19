// TinyUSB device configuration for pico_toolset_usb_composite (CDC + MSC).
// Must be the only tusb_config.h in the final build (see usb_hid's
// tusb_config.h for why): tinyusb_device is an INTERFACE library whose
// sources compile into the final executable.
#ifndef TUSB_CONFIG_H_
#define TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CFG_TUSB_RHPORT0_MODE
#define CFG_TUSB_RHPORT0_MODE (OPT_MODE_DEVICE)
#endif
#ifndef CFG_TUSB_OS
#define CFG_TUSB_OS OPT_OS_PICO
#endif
#define CFG_TUSB_MEM_SECTION
#ifndef CFG_TUSB_MEM_ALIGN
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))
#endif

#ifndef CFG_TUD_ENDPOINT0_SIZE
#define CFG_TUD_ENDPOINT0_SIZE 64
#endif
#ifndef CFG_TUD_CDC
#define CFG_TUD_CDC 1
#endif
#ifndef CFG_TUD_MSC
#define CFG_TUD_MSC 1
#endif
#ifndef CFG_TUD_HID
#define CFG_TUD_HID 0
#endif
#ifndef CFG_TUD_MIDI
#define CFG_TUD_MIDI 0
#endif
#ifndef CFG_TUD_VENDOR
#define CFG_TUD_VENDOR 0
#endif

#ifndef CFG_TUD_CDC_RX_BUFSIZE
#define CFG_TUD_CDC_RX_BUFSIZE 256
#endif
#ifndef CFG_TUD_CDC_TX_BUFSIZE
#define CFG_TUD_CDC_TX_BUFSIZE 512
#endif
#ifndef CFG_TUD_CDC_EP_BUFSIZE
#define CFG_TUD_CDC_EP_BUFSIZE 64
#endif
#ifndef CFG_TUD_MSC_EP_BUFSIZE
#define CFG_TUD_MSC_EP_BUFSIZE 4096 // multiple sectors per SD transfer (read10/write10 handle bufsize % 512 == 0)
#endif

#ifdef __cplusplus
}
#endif
#endif
