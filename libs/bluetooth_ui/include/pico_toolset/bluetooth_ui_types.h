#pragma once

#include <cstddef>
#include <cstdint>

namespace pico_toolset {

// What the Bluetooth page shows about one device, whichever component manages it.
enum class BtUiSource : uint8_t { Hid, Audio };
enum class BtUiKind : uint8_t { Unknown, Keyboard, Mouse, KeyboardMouse, Gamepad, Headset };
enum class BtUiState : uint8_t { Offline, Connecting, Connected, Streaming };

struct BtUiDevice {
    uint8_t    addr[6];
    uint8_t    addr_type;   // as the owning component wants it back (HID: 0 public, 1 random, 2 Classic)
    char       name[24];    // empty = unknown
    BtUiSource source;
    BtUiKind   kind;
    BtUiState  state;
    int8_t     rssi;        // found devices only, else 0
};

[[nodiscard]] const char* bt_ui_kind_name(BtUiKind kind);   // "Keyboard", "Mouse", ...
[[nodiscard]] const char* bt_ui_state_name(BtUiState state); // "Offline", "Connecting", ...
[[nodiscard]] bool bt_ui_same_device(const BtUiDevice& a, const BtUiDevice& b);

// One per component that manages devices (see bluetooth_ui_hid.h / bluetooth_ui_a2dp.h). The page
// only talks to this, so it is testable on a PC and a further kind of device is one more adaptor.
// All methods are called from the application thread.
class BtUiBackend {
public:
    virtual ~BtUiBackend() = default;

    [[nodiscard]] virtual BtUiSource source() const = 0;
    // Changes whenever anything this backend reports may have changed.
    [[nodiscard]] virtual uint32_t change_counter() const = 0;

    virtual size_t bonded(BtUiDevice* out, size_t max) = 0;
    virtual size_t found(BtUiDevice* out, size_t max) = 0;

    [[nodiscard]] virtual bool scanning() const = 0;
    virtual void start_scan() = 0;
    virtual void stop_scan() = 0;

    // Pairs a found device, or connects a bonded one.
    virtual bool connect(const BtUiDevice& device) = 0;
    [[nodiscard]] virtual bool can_disconnect(const BtUiDevice&) const { return false; }
    virtual void disconnect(const BtUiDevice&) {}
    virtual bool forget(const BtUiDevice& device) = 0;

    [[nodiscard]] virtual bool connecting() const { return false; }
    // A keyboard being paired waits for this code to be typed on it.
    virtual bool passkey(uint32_t*) const { return false; }
    [[nodiscard]] virtual uint8_t last_error() const { return 0; } // BTstack/HCI status, 0 = none
};

} // namespace pico_toolset
