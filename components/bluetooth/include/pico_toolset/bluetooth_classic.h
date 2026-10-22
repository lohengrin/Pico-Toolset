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
    static void init(const char* pin = "0000");

    // Passkey shown by a device that asks the user to type it on the other side (SSP passkey
    // entry); several listeners may register (up to 2).
    static void add_passkey_listener(ClassicPasskeyCallback callback, void* user);

    // One inquiry at a time. `major_class` filters results by the major device class of the
    // class of device (0x05 peripheral, 0x04 audio/video); 0 = report everything. Returns
    // false if an inquiry is already running or the command could not be started.
    // duration_1280ms is the length in 1.28 s units (1..48).
    static bool start_inquiry(uint8_t duration_1280ms, uint8_t major_class, ClassicInquiryCallback callback,
                              void* user);
    static void stop_inquiry();
    [[nodiscard]] static bool inquiry_active();

    // Paging a bonded device in the background interrupts an established link (audible as
    // dropouts in an A2DP stream): while any component holds this, the others should not page.
    static void inhibit_background_paging(bool inhibit);
    // Called when inhibiting starts, so a component can cancel a page already in flight.
    static void set_paging_inhibit_listener(void (*callback)(void*), void* user);
    [[nodiscard]] static bool background_paging_inhibited();
};

} // namespace pico_toolset
