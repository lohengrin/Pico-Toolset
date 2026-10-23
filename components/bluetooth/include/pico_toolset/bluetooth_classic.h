#pragma once

// Shared Bluetooth Classic (BR/EDR) plumbing for the components that use it (bluetooth_hid,
// bluetooth_a2dp): GAP defaults, automatic answers to pairing requests, and a single
// inquiry whose results are filtered by major device class. Compiled only when the build
// links BTstack's Classic support (CMake option PICO_TOOLSET_BLUETOOTH_CLASSIC).
//
// All functions must be called from the BTstack context or with the async-context lock held
// (as the components that use them do), and only after Bluetooth::init().

#include <cstdint>

namespace pico_toolset {

struct ClassicInquiryResult {
    uint8_t  addr[6];
    uint32_t class_of_device;
    bool     rssi_valid;
    int8_t   rssi;
    char     name[24];       // empty unless the inquiry data (EIR) carries a name
    uint16_t vendor_id;      // Device ID profile, 0 if absent
    uint16_t product_id;
};

using ClassicInquiryCallback = void (*)(const ClassicInquiryResult& result, void* user);
using ClassicPasskeyCallback = void (*)(const uint8_t addr[6], uint32_t passkey, void* user);

class BluetoothClassic {
public:
    // Idempotent. Sets the Classic GAP defaults (bondable, Secure Simple Pairing with
    // display-only I/O, sniff + role switch allowed, we become master, connectable) and starts
    // answering pairing requests: legacy PIN requests with `pin`, numeric-comparison /
    // just-works confirmations with "yes", passkey requests with "no" (we have no keyboard).
    static void init(const char* pin = "0000"); // `pin` is copied (max 16 chars)

    // Pairing is answered (PIN, SSP confirmation) only while at least one registered gate reports
    // an open pairing window; with no gate registered everything is answered. A component
    // registers one that is true while it is pairing or connecting a new device (up to 2).
    static void add_pairing_gate(bool (*is_open)(void*), void* user);

    // Passkey shown by a device that asks the user to type it on the other side (SSP passkey
    // entry); several listeners may register (up to 2).
    static void add_passkey_listener(ClassicPasskeyCallback callback, void* user);

    // Device inquiry, shared by up to 2 clients (e.g. bluetooth_hid looking for a pad while
    // bluetooth_a2dp looks for a headset): there is one radio inquiry, repeated for as long as any
    // client is registered, and every client gets the results of its own `major_class` (class of
    // device bits 8..12: 0x05 peripheral, 0x04 audio/video; 0 = everything). A client is
    // identified by its `callback`; starting again with the same callback replaces it. Clients run
    // their own timeouts and call stop_inquiry(callback); the radio inquiry stops with the last one.
    // Returns false if all client slots are taken or the command could not be started.
    // duration_1280ms is the length of one round in 1.28 s units (1..48).
    static bool start_inquiry(uint8_t duration_1280ms, uint8_t major_class, ClassicInquiryCallback callback,
                              void* user);
    static void stop_inquiry(ClassicInquiryCallback callback);
    [[nodiscard]] static bool inquiry_active();

    // Paging a bonded device in the background interrupts an established link (audible as
    // dropouts in an A2DP stream): while any component holds this, the others should not page.
    static void inhibit_background_paging(bool inhibit);
    // Called whenever the flag changes (`busy` = true when a stream needs the radio), so a component
    // can cancel a page already in flight and throttle its own scanning while it lasts.
    static void set_paging_inhibit_listener(void (*callback)(bool busy, void* user), void* user);
    [[nodiscard]] static bool background_paging_inhibited();
};

} // namespace pico_toolset
