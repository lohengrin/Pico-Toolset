#pragma once

#include "pico_toolset/bluetooth_ui_types.h"

namespace pico_toolset {

// Merges the devices of several backends (HID, audio, ...) into one list and routes the commands.
class BluetoothDevices {
public:
    static constexpr size_t kMaxBackends = 4;
    static constexpr size_t kMaxDevices = 8;
    static constexpr size_t kMaxFound = 16;

    // The backend must outlive this object.
    bool add_backend(BtUiBackend& backend);

    // Sum over the backends and this object's own changes: differs after anything visible moved.
    [[nodiscard]] uint32_t change_counter() const;

    size_t bonded(BtUiDevice* out, size_t max) const;
    // Devices seen while scanning, minus those already bonded.
    size_t found(BtUiDevice* out, size_t max) const;

    [[nodiscard]] bool scanning() const;
    void start_scan();   // every backend scans at once, so one list covers all kinds
    void stop_scan();

    bool connect(const BtUiDevice& device);
    [[nodiscard]] bool can_disconnect(const BtUiDevice& device) const;
    void disconnect(const BtUiDevice& device);
    bool forget(const BtUiDevice& device);

    [[nodiscard]] bool connecting() const;
    bool passkey(uint32_t* code) const;
    [[nodiscard]] uint8_t last_error() const;

private:
    BtUiBackend* backend_for(BtUiSource source) const;

    BtUiBackend* backends_[kMaxBackends]{};
    size_t count_ = 0;
    mutable uint32_t local_changes_ = 0;
};

} // namespace pico_toolset
