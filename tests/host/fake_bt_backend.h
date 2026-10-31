#pragma once
// A scriptable BtUiBackend and a device builder, shared by the host tests of the Bluetooth page views.
#include <cstring>
#include <vector>

#include "pico_toolset/bluetooth_ui_types.h"

namespace pico_toolset {

inline BtUiDevice make_device(BtUiSource src, uint8_t id, const char* name, BtUiKind kind, BtUiState state = BtUiState::Offline) {
    BtUiDevice d{};
    d.addr[5] = id;
    std::strncpy(d.name, name, sizeof(d.name) - 1);
    d.source = src;
    d.kind = kind;
    d.state = state;
    return d;
}

class FakeBackend final : public BtUiBackend {
public:
    FakeBackend(BtUiSource s, bool can_disc) : src(s), can_disc(can_disc) {}
    BtUiSource src;
    bool can_disc;
    std::vector<BtUiDevice> paired, seen;
    bool scan = false, busy = false;
    uint8_t error = 0;
    uint32_t counter = 0;
    bool pair_succeeds = true;
    int connects = 0, forgets = 0, disconnects = 0;

    BtUiSource source() const override { return src; }
    uint32_t change_counter() const override { return counter; }
    size_t bonded(BtUiDevice* out, size_t max) override {
        size_t n = 0;
        for (; n < paired.size() && n < max; ++n) out[n] = paired[n];
        return n;
    }
    size_t found(BtUiDevice* out, size_t max) override {
        size_t n = 0;
        for (; n < seen.size() && n < max; ++n) out[n] = seen[n];
        return n;
    }
    bool scanning() const override { return scan; }
    void start_scan() override { scan = true; seen.clear(); ++counter; }
    void stop_scan() override { scan = false; ++counter; }
    bool connect(const BtUiDevice& d) override {
        ++connects;
        scan = false;
        error = 0;
        for (auto& p : paired)
            if (bt_ui_same_device(p, d)) { p.state = BtUiState::Connected; ++counter; return true; }
        if (pair_succeeds) {
            BtUiDevice p = d;
            p.state = BtUiState::Connected;
            paired.push_back(p);
        } else {
            error = 0x05;
        }
        ++counter;
        return true;
    }
    bool can_disconnect(const BtUiDevice&) const override { return can_disc; }
    void disconnect(const BtUiDevice& d) override {
        ++disconnects;
        for (auto& p : paired) if (bt_ui_same_device(p, d)) p.state = BtUiState::Offline;
        ++counter;
    }
    bool forget(const BtUiDevice& d) override {
        ++forgets;
        for (size_t i = 0; i < paired.size(); ++i)
            if (bt_ui_same_device(paired[i], d)) { paired.erase(paired.begin() + i); ++counter; return true; }
        return false;
    }
    bool connecting() const override { return busy; }
    bool passkey(uint32_t* c) const override {
        if (!passkey_value) return false;
        if (c) *c = passkey_value;
        return true;
    }
    uint8_t last_error() const override { return error; }
    uint32_t passkey_value = 0;
};


} // namespace pico_toolset
