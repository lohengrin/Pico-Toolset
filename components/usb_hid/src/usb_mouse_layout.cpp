#include "pico_toolset/usb_mouse_layout.h"

namespace pico_toolset {

namespace {

int32_t item_value(const uint8_t* p, unsigned size, bool is_signed) {
    uint32_t v = 0;
    for (unsigned i = 0; i < size; ++i) v |= static_cast<uint32_t>(p[i]) << (8 * i);
    if (is_signed && size && size < 4 && (v & (1u << (8 * size - 1)))) v |= ~0u << (8 * size);
    return static_cast<int32_t>(v);
}

uint32_t get_bits(const uint8_t* data, unsigned bit, unsigned size) {
    uint32_t v = 0;
    for (unsigned i = 0; i < size; ++i)
        if (data[(bit + i) >> 3] & (1u << ((bit + i) & 7))) v |= 1u << i;
    return v;
}

int32_t sign_extend(uint32_t v, unsigned size) {
    if (size && size < 32 && (v & (1u << (size - 1)))) v |= ~0u << size;
    return static_cast<int32_t>(v);
}

} // namespace

bool usb_mouse_parse_descriptor(const uint8_t* d, uint16_t len, UsbMouseLayout& out) {
    out = UsbMouseLayout{};

    // Global state
    uint32_t usage_page = 0;
    int32_t logical_min = 0;
    uint32_t report_size = 0, report_count = 0;
    uint8_t report_id = 0;
    // Local state (cleared after every main item)
    uint32_t usages[16];
    unsigned usage_count = 0;
    uint32_t usage_min = 0;
    bool have_range = false;

    // Bit offsets per report id (after the id byte); a handful of ids is plenty for a mouse.
    struct Offset { uint8_t id; uint32_t bits; };
    Offset offsets[8] = {};
    unsigned offset_count = 0;
    auto offset_for = [&](uint8_t id) -> uint32_t& {
        for (unsigned i = 0; i < offset_count; ++i)
            if (offsets[i].id == id) return offsets[i].bits;
        if (offset_count < 8) { offsets[offset_count] = {id, 0}; return offsets[offset_count++].bits; }
        return offsets[7].bits;
    };

    unsigned depth = 0;
    bool in_mouse = false;
    bool id_chosen = false;
    uint8_t mouse_id = 0;
    bool seen_x = false, seen_y = false;

    for (uint16_t i = 0; i < len;) {
        const uint8_t prefix = d[i];
        if (prefix == 0xFE) { // long item: skip
            if (i + 2 >= len) break;
            i = static_cast<uint16_t>(i + 3 + d[i + 1]);
            continue;
        }
        const unsigned size = (prefix & 3) == 3 ? 4 : (prefix & 3);
        const unsigned type = (prefix >> 2) & 3;
        const unsigned tag = prefix >> 4;
        if (i + 1 + size > len) break;
        const uint8_t* data = d + i + 1;
        i = static_cast<uint16_t>(i + 1 + size);

        if (type == 1) { // global
            switch (tag) {
            case 0x0: usage_page = static_cast<uint32_t>(item_value(data, size, false)); break;
            case 0x1: logical_min = item_value(data, size, true); break;
            case 0x7: report_size = static_cast<uint32_t>(item_value(data, size, false)); break;
            case 0x8: report_id = static_cast<uint8_t>(item_value(data, size, false)); break;
            case 0x9: report_count = static_cast<uint32_t>(item_value(data, size, false)); break;
            default: break;
            }
        } else if (type == 2) { // local
            const uint32_t v = static_cast<uint32_t>(item_value(data, size, false));
            if (tag == 0x0 && usage_count < 16) usages[usage_count++] = size == 4 ? v : ((usage_page << 16) | v);
            else if (tag == 0x1) { usage_min = size == 4 ? v : ((usage_page << 16) | v); have_range = true; }
            else if (tag == 0x2) have_range = true; // the range's end is implied by report_count
        } else if (type == 0) { // main
            if (tag == 0xA) { // collection
                if (depth == 0) in_mouse = usage_count > 0 && usages[0] == 0x00010002;
                ++depth;
            } else if (tag == 0xC) { // end collection
                if (depth > 0) --depth;
                if (depth == 0) in_mouse = false;
            } else if (tag == 0x8) { // input
                const uint32_t flags = static_cast<uint32_t>(item_value(data, size, false));
                uint32_t& bit = offset_for(report_id);
                const bool constant = flags & 1;
                for (uint32_t f = 0; f < report_count; ++f) {
                    if (!constant && in_mouse && (!id_chosen || mouse_id == report_id)) {
                        uint32_t usage = 0;
                        if (have_range) usage = usage_min + f;
                        else if (usage_count) usage = usages[f < usage_count ? f : usage_count - 1];
                        const bool is_signed = logical_min < 0;
                        if ((usage >> 16) == 9 && report_size == 1) { // buttons
                            if (out.buttons_count == 0) out.buttons_bit = static_cast<uint16_t>(bit);
                            if (out.buttons_count < 255) ++out.buttons_count;
                            id_chosen = true;
                            mouse_id = report_id;
                        } else if (usage == 0x00010030 && report_size <= 16 && !seen_x) {
                            out.x_bit = static_cast<uint16_t>(bit); out.x_size = static_cast<uint8_t>(report_size);
                            out.x_signed = is_signed; seen_x = true; id_chosen = true; mouse_id = report_id;
                        } else if (usage == 0x00010031 && report_size <= 16 && !seen_y) {
                            out.y_bit = static_cast<uint16_t>(bit); out.y_size = static_cast<uint8_t>(report_size);
                            out.y_signed = is_signed; seen_y = true; id_chosen = true; mouse_id = report_id;
                        } else if (usage == 0x00010038 && report_size <= 16 && !out.has_wheel) {
                            out.wheel_bit = static_cast<uint16_t>(bit); out.wheel_size = static_cast<uint8_t>(report_size);
                            out.wheel_signed = is_signed; out.has_wheel = true; id_chosen = true; mouse_id = report_id;
                        }
                    }
                    bit += report_size;
                }
            }
            usage_count = 0;
            have_range = false;
        }
    }

    out.valid = seen_x && seen_y;
    out.report_id = id_chosen ? mouse_id : 0;
    if (out.valid) {
        uint32_t end = out.x_bit + out.x_size;
        end = end < out.y_bit + out.y_size ? out.y_bit + out.y_size : end;
        if (out.has_wheel && end < static_cast<uint32_t>(out.wheel_bit + out.wheel_size)) end = out.wheel_bit + out.wheel_size;
        if (out.buttons_count && end < static_cast<uint32_t>(out.buttons_bit + out.buttons_count)) end = out.buttons_bit + out.buttons_count;
        out.report_bytes = static_cast<uint16_t>(((end + 7) >> 3) + (out.report_id ? 1 : 0));
    }
    return out.valid;
}

bool usb_mouse_parse_report(const UsbMouseLayout& l, const uint8_t* report, uint16_t len, UsbMouseReport& out) {
    if (!l.valid || len < l.report_bytes) return false;
    const uint8_t* data = report;
    if (l.report_id) {
        if (report[0] != l.report_id) return false;
        data = report + 1;
    }
    out = UsbMouseReport{};
    for (unsigned b = 0; b < l.buttons_count && b < 3; ++b)
        if (get_bits(data, l.buttons_bit + b, 1)) out.buttons = static_cast<uint8_t>(out.buttons | (1u << b));
    auto axis = [&](uint16_t bit, uint8_t size, bool is_signed) {
        const uint32_t raw = get_bits(data, bit, size);
        return is_signed ? sign_extend(raw, size) : static_cast<int32_t>(raw);
    };
    out.dx = axis(l.x_bit, l.x_size, l.x_signed);
    out.dy = axis(l.y_bit, l.y_size, l.y_signed);
    if (l.has_wheel) out.wheel = axis(l.wheel_bit, l.wheel_size, l.wheel_signed);
    return true;
}

} // namespace pico_toolset
