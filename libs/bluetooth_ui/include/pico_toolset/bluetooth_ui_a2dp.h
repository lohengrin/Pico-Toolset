#pragma once

#include <cstring>

#include "pico_toolset/bluetooth_a2dp.h"
#include "pico_toolset/bluetooth_ui_types.h"

namespace pico_toolset {

// Lets the Bluetooth page manage the headset of a BluetoothA2dp (one remembered headset). Configure
// it with auto_connect = false so the user chooses which found headset to connect.
class BtUiA2dpBackend final : public BtUiBackend {
public:
    explicit BtUiA2dpBackend(BluetoothA2dp& a2dp) : a2dp_(a2dp) {}

    BtUiSource source() const override { return BtUiSource::Audio; }
    uint32_t change_counter() const override { return a2dp_.change_counter(); }

    size_t bonded(BtUiDevice* out, size_t max) override {
        if (!max || !a2dp_.has_last_device()) return 0;
        const A2dpDevice last = a2dp_.last_device();
        out[0] = convert(last);
        A2dpDevice current{};
        if (a2dp_.current_device(&current) && std::memcmp(current.addr, last.addr, 6) == 0) {
            switch (a2dp_.state()) {
            case A2dpState::Connecting: out[0].state = BtUiState::Connecting; break;
            case A2dpState::Connected: out[0].state = BtUiState::Connected; break;
            case A2dpState::Streaming: out[0].state = BtUiState::Streaming; break;
            default: break;
            }
        }
        return 1;
    }
    size_t found(BtUiDevice* out, size_t max) override {
        A2dpDevice tmp[BluetoothA2dp::kMaxFound];
        const size_t n = a2dp_.found_devices(tmp, BluetoothA2dp::kMaxFound);
        size_t k = 0;
        for (; k < n && k < max; ++k) {
            out[k] = convert(tmp[k]);
            out[k].rssi = tmp[k].rssi;
        }
        return k;
    }

    bool scanning() const override { return a2dp_.state() == A2dpState::Pairing; }
    void start_scan() override { a2dp_.start_pairing(); }
    void stop_scan() override { a2dp_.stop_pairing(); }

    bool connect(const BtUiDevice& d) override { return a2dp_.connect(d.addr); }
    bool can_disconnect(const BtUiDevice&) const override {
        return a2dp_.state() == A2dpState::Connected || a2dp_.state() == A2dpState::Streaming;
    }
    void disconnect(const BtUiDevice&) override { a2dp_.disconnect(); }
    bool forget(const BtUiDevice&) override { a2dp_.forget_last(); return true; }

    bool connecting() const override { return a2dp_.state() == A2dpState::Connecting; }
    uint8_t last_error() const override { return a2dp_.last_error(); }

private:
    static BtUiDevice convert(const A2dpDevice& a) {
        BtUiDevice d{};
        std::memcpy(d.addr, a.addr, 6);
        std::memcpy(d.name, a.name, sizeof(d.name) < sizeof(a.name) ? sizeof(d.name) : sizeof(a.name));
        d.name[sizeof(d.name) - 1] = '\0';
        d.source = BtUiSource::Audio;
        d.kind = BtUiKind::Headset;
        d.state = BtUiState::Offline;
        return d;
    }

    BluetoothA2dp& a2dp_;
};

} // namespace pico_toolset
