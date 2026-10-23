#pragma once
// Internal helpers shared by the Bluetooth components (bluetooth, bluetooth_hid, bluetooth_a2dp).
// Not part of the public API: include from .cpp files only (pulls in BTstack and the CYW43 arch).

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "btstack_tlv.h"
#include "pico/async_context.h"
#include "pico/cyw43_arch.h"

namespace pico_toolset::bt_detail {

// Holds the async-context lock around BTstack calls made from application code (BTstack is not
// thread-safe; its own callbacks already run under it). Recursive, so it is also safe inside them.
struct ContextLock {
    ContextLock() { async_context_acquire_lock_blocking(cyw43_arch_async_context()); }
    ~ContextLock() { async_context_release_lock(cyw43_arch_async_context()); }
};

// Local name (shortened 0x08 or complete 0x09) from advertising data, NUL-terminated and truncated
// to out_size - 1; `out` is left empty when there is none.
inline void extract_local_name(const uint8_t* ad, uint8_t len, char* out, size_t out_size) {
    out[0] = '\0';
    for (unsigned i = 0; i + 1 < len;) {
        const unsigned field_len = ad[i];
        if (field_len == 0 || i + 1 + field_len > len) break;
        const uint8_t type = ad[i + 1];
        if (type == 0x08 || type == 0x09) {
            const size_t n = field_len - 1 < out_size - 1 ? field_len - 1 : out_size - 1;
            std::memcpy(out, &ad[i + 2], n);
            out[n] = '\0';
            if (type == 0x09) return;
        }
        i += field_len + 1;
    }
}

// One small record per tag in BTstack's TLV store (flash, next to the link keys).
constexpr uint32_t tlv_tag(char a, char b, char c, char d) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(c) << 8) |
           static_cast<uint32_t>(d);
}
inline bool tlv_store(uint32_t tag, const void* data, uint32_t len) {
    const btstack_tlv_t* tlv = nullptr;
    void* ctx = nullptr;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (!tlv) return false;
    tlv->store_tag(ctx, tag, static_cast<const uint8_t*>(data), len);
    return true;
}
// Bytes read, or 0 when the tag is missing / no store is set up.
inline int tlv_load(uint32_t tag, void* data, uint32_t capacity) {
    const btstack_tlv_t* tlv = nullptr;
    void* ctx = nullptr;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (!tlv) return 0;
    const int n = tlv->get_tag(ctx, tag, static_cast<uint8_t*>(data), capacity);
    return n > 0 ? n : 0;
}
inline void tlv_erase(uint32_t tag) {
    const btstack_tlv_t* tlv = nullptr;
    void* ctx = nullptr;
    btstack_tlv_get_instance(&tlv, &ctx);
    if (tlv) tlv->delete_tag(ctx, tag);
}

} // namespace pico_toolset::bt_detail
