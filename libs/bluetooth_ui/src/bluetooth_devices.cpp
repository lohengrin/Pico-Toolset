#include "pico_toolset/bluetooth_devices.h"

#include <cstring>

namespace pico_toolset {

const char* bt_ui_kind_name(BtUiKind kind) {
    switch (kind) {
    case BtUiKind::Keyboard: return "Keyboard";
    case BtUiKind::Mouse: return "Mouse";
    case BtUiKind::KeyboardMouse: return "Keyboard+Mouse";
    case BtUiKind::Gamepad: return "Gamepad";
    case BtUiKind::Headset: return "Headset";
    default: return "Device";
    }
}

const char* bt_ui_state_name(BtUiState state) {
    switch (state) {
    case BtUiState::Connecting: return "Connecting";
    case BtUiState::Connected: return "Connected";
    case BtUiState::Streaming: return "Streaming";
    default: return "Offline";
    }
}

bool bt_ui_same_device(const BtUiDevice& a, const BtUiDevice& b) {
    return a.source == b.source && std::memcmp(a.addr, b.addr, 6) == 0;
}

bool BluetoothDevices::add_backend(BtUiBackend& backend) {
    if (count_ >= kMaxBackends) return false;
    backends_[count_++] = &backend;
    return true;
}

uint32_t BluetoothDevices::change_counter() const {
    uint32_t sum = local_changes_;
    for (size_t i = 0; i < count_; ++i) sum += backends_[i]->change_counter();
    return sum;
}

BtUiBackend* BluetoothDevices::backend_for(BtUiSource source) const {
    for (size_t i = 0; i < count_; ++i)
        if (backends_[i]->source() == source) return backends_[i];
    return nullptr;
}

size_t BluetoothDevices::bonded(BtUiDevice* out, size_t max) const {
    size_t n = 0;
    for (size_t i = 0; i < count_ && n < max; ++i) n += backends_[i]->bonded(out + n, max - n);
    return n;
}

size_t BluetoothDevices::found(BtUiDevice* out, size_t max) const {
    BtUiDevice known[kMaxDevices];
    const size_t known_count = bonded(known, kMaxDevices);
    size_t n = 0;
    for (size_t i = 0; i < count_ && n < max; ++i) {
        BtUiDevice tmp[kMaxFound];
        const size_t got = backends_[i]->found(tmp, kMaxFound);
        for (size_t k = 0; k < got && n < max; ++k) {
            bool skip = false;
            for (size_t b = 0; b < known_count; ++b)
                if (std::memcmp(known[b].addr, tmp[k].addr, 6) == 0) skip = true;
            for (size_t o = 0; o < n; ++o) // the same device can answer both an LE and a Classic scan
                if (std::memcmp(out[o].addr, tmp[k].addr, 6) == 0) skip = true;
            if (!skip) out[n++] = tmp[k];
        }
    }
    return n;
}

bool BluetoothDevices::scanning() const {
    for (size_t i = 0; i < count_; ++i)
        if (backends_[i]->scanning()) return true;
    return false;
}

void BluetoothDevices::start_scan() {
    for (size_t i = 0; i < count_; ++i) backends_[i]->start_scan();
    ++local_changes_;
}

void BluetoothDevices::stop_scan() {
    for (size_t i = 0; i < count_; ++i) backends_[i]->stop_scan();
    ++local_changes_;
}

bool BluetoothDevices::connect(const BtUiDevice& device) {
    BtUiBackend* b = backend_for(device.source);
    if (!b) return false;
    // A connection attempt ends every scan: the radio does one thing at a time.
    for (size_t i = 0; i < count_; ++i)
        if (backends_[i] != b && backends_[i]->scanning()) backends_[i]->stop_scan();
    ++local_changes_;
    return b->connect(device);
}

bool BluetoothDevices::can_disconnect(const BtUiDevice& device) const {
    const BtUiBackend* b = backend_for(device.source);
    return b && b->can_disconnect(device);
}

void BluetoothDevices::disconnect(const BtUiDevice& device) {
    if (BtUiBackend* b = backend_for(device.source)) b->disconnect(device);
    ++local_changes_;
}

bool BluetoothDevices::forget(const BtUiDevice& device) {
    BtUiBackend* b = backend_for(device.source);
    ++local_changes_;
    return b && b->forget(device);
}

bool BluetoothDevices::connecting() const {
    for (size_t i = 0; i < count_; ++i)
        if (backends_[i]->connecting()) return true;
    return false;
}

bool BluetoothDevices::passkey(uint32_t* code) const {
    for (size_t i = 0; i < count_; ++i)
        if (backends_[i]->passkey(code)) return true;
    return false;
}

uint8_t BluetoothDevices::last_error() const {
    for (size_t i = 0; i < count_; ++i)
        if (const uint8_t e = backends_[i]->last_error()) return e;
    return 0;
}

} // namespace pico_toolset
