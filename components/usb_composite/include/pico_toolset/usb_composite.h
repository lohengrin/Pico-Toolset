#pragma once

#include <cstdint>

namespace pico_toolset {

// Block-device backend for the MSC interface (512-byte blocks). Plain
// function pointers + context so the component has zero knowledge of what
// storage sits behind it (SD card, RAM disk, ...).
struct UsbBlockDevice {
    void* ctx = nullptr;
    bool (*ready)(void* ctx) = nullptr;                  // media present/usable?
    uint32_t (*block_count)(void* ctx) = nullptr;
    bool (*read)(void* ctx, uint32_t lba, uint8_t* buf, uint32_t count) = nullptr;
    bool (*write)(void* ctx, uint32_t lba, const uint8_t* buf, uint32_t count) = nullptr;
    // Flush anything the medium still has in flight (called on SYNCHRONIZE CACHE
    // and on eject, so a host's "safely remove" really is safe). null = writes
    // are already durable when write() returns.
    bool (*sync)(void* ctx) = nullptr;
    // false reports the medium as write-protected to the host (MODE SENSE, and
    // writes are refused). null = writable. Polled per command, so it may change.
    bool (*writable)(void* ctx) = nullptr;
};

struct UsbCompositeConfig {
    UsbBlockDevice block_device;
    const char* manufacturer = "Pico-Toolset";
    const char* product = "Pico composite";
    const char* msc_vendor = "Pico";           // <= 8 chars
    const char* msc_product = "Storage";       // <= 16 chars
    uint16_t vid = 0x2E8A;                     // Raspberry Pi
    uint16_t pid = 0x000A;                     // pico-sdk CDC PID: covered by picotool's stock udev rules (60-picotool.rules) -- override for a product
    bool install_stdio = true;                 // route printf/getchar over the CDC interface
};

// TinyUSB *device* stack, CDC (serial) + MSC (mass storage) composite on
// the native USB port (rhport 0). Not compatible with pico_stdio_usb (which
// brings its own descriptors) nor with pico_toolset_usb_hid's TinyUSB
// config (host stack) -- see usb_composite/README.md.
void usb_composite_init(const UsbCompositeConfig& config);

// Must be called frequently (main loop). With install_stdio, the stdio
// input path (getchar etc.) already pumps it while waiting.
void usb_composite_task();

// Tell the host the medium changed (card swapped, remounted, ...): the next
// TEST UNIT READY reports UNIT ATTENTION "media may have changed" and the
// capacity is read again. Also clears a host-issued eject.
void usb_composite_media_changed();

// True after the host ejected the medium ("safely remove") and until
// usb_composite_media_changed() or a host load command.
bool usb_composite_ejected();

// True while a host has the CDC port open (DTR asserted).
bool usb_composite_cdc_connected();

} // namespace pico_toolset
