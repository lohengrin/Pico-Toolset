#pragma once

#include <cstring>

#include "pico_toolset/bluetooth_hid.h"
#include "pico_toolset/bluetooth_ui_types.h"

namespace pico_toolset {

// Lets the Bluetooth page manage the devices of a BluetoothHid. Configure it with auto_pair = false
// so the user chooses which found device to pair; with auto_pair the first one is paired by itself.
// HID devices reconnect by themselves, so there is no disconnect.
class BtUiHidBackend final : public BtUiBackend {
public:
    explicit BtUiHidBackend(BluetoothHid& hid) : hid_(hid) {}

    BtUiSource source() const override { return BtUiSource::Hid; }
    uint32_t change_counter() const override { return hid_.change_counter(); }

    size_t bonded(BtUiDevice* out, size_t max) override {
        BtHidDevice tmp[BluetoothHid::kMaxBonded];
        const size_t n = hid_.bonded_devices(tmp, BluetoothHid::kMaxBonded);
        size_t k = 0;
        for (; k < n && k < max; ++k) out[k] = convert(tmp[k]);
        return k;
    }
    size_t found(BtUiDevice* out, size_t max) override {
        BtHidDevice tmp[BluetoothHid::kMaxFound];
        const size_t n = hid_.found_devices(tmp, BluetoothHid::kMaxFound);
        size_t k = 0;
        for (; k < n && k < max; ++k) out[k] = convert(tmp[k]);
        return k;
    }

    bool scanning() const override { return hid_.pairing(); }
    void start_scan() override { hid_.start_pairing(); }
    void stop_scan() override { hid_.stop_pairing(); }

    bool connect(const BtUiDevice& d) override { return hid_.pair(d.addr, d.addr_type); }
    bool forget(const BtUiDevice& d) override { return hid_.forget(d.addr); }

    bool connecting() const override { return hid_.connecting(); }
    bool passkey(uint32_t* code) const override { return hid_.pairing_passkey(code); }
    uint8_t last_error() const override { return hid_.last_pairing_error(); }

private:
    static BtUiDevice convert(const BtHidDevice& h) {
        BtUiDevice d{};
        std::memcpy(d.addr, h.addr, 6);
        d.addr_type = h.addr_type;
        std::memcpy(d.name, h.name, sizeof(d.name) < sizeof(h.name) ? sizeof(d.name) : sizeof(h.name));
        d.name[sizeof(d.name) - 1] = '\0';
        d.source = BtUiSource::Hid;
        const bool pad = h.kinds & bt_hid_kind::Gamepad;
        const bool kbd = h.kinds & bt_hid_kind::Keyboard;
        const bool mouse = h.kinds & bt_hid_kind::Mouse;
        d.kind = pad ? BtUiKind::Gamepad : kbd && mouse ? BtUiKind::KeyboardMouse
               : kbd ? BtUiKind::Keyboard : mouse ? BtUiKind::Mouse : BtUiKind::Unknown;
        d.state = h.connected ? BtUiState::Connected : BtUiState::Offline;
        d.rssi = h.rssi;
        return d;
    }

    BluetoothHid& hid_;
};

} // namespace pico_toolset
