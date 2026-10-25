#if PICO_RP2350

#include "pico_toolset/st7701_indexed.h"

#include "pico/platform.h"
#include "pico_toolset/st7701_indexed_rows.h"

namespace pico_toolset {

// One panel line, under the scanout deadline of ~one line time: RAM code, SRAM data, word-wide moves.
void __not_in_flash_func(St7701IndexedSource::line_callback)(int y, uint16_t* line, void* user) {
    const St7701IndexedSource& s = *static_cast<const St7701IndexedSource*>(user);
    const uint8_t* src = s.m_source;
    uint32_t* d = reinterpret_cast<uint32_t*>(line);
    if (!src) {
        for (int i = 0; i < 480 / 2; ++i) d[i] = 0;
        return;
    }
    if (s.m_width == 480) {
        const int oy = y - s.m_overlay_first;
        const uint8_t* row = (oy >= 0 && oy < s.m_overlay_count) ? s.m_overlay + oy * 480 : src + y * 480;
        indexed_row_to_pixels(d, row, s.m_lut);
    } else {
        indexed_row_to_pixels_1p5(d, src + indexed_source_row_1p5(y) * 320, s.m_lut);
    }
}

} // namespace pico_toolset

#endif // PICO_RP2350
