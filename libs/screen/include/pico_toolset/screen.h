#pragma once

#include <cstdint>
#include <vector>

#include "pico_toolset/display_driver.h"
#include "pico_toolset/widget.h"

namespace pico_toolset {

// Composes widgets onto a DisplayDriver using PiCoMonitor-style slots.
//
// Each slot owns an (optional) widget: UL/UR/BL/BR place a widget in a
// quadrant of the display; FS owns a full-screen widget. Render order is
// UL, UR, BL, BR, FS (FS last => drawn on top). Slot ordinals are stable, so
// a widget can be swapped, added or removed at runtime and draw() handles
// the rest.
class Screen {
public:
    enum Slot : uint8_t {
        UL = 0, // upper left
        UR = 1, // upper right
        BL = 2, // bottom left
        BR = 3, // bottom right
        FS = 4, // full screen
        kSlotCount,
    };

    explicit Screen(DisplayDriver& driver) : m_driver(driver) {}

    DisplayDriver& driver() { return m_driver; }
    const DisplayDriver& driver() const { return m_driver; }

    // Widget ownership: the Screen does NOT free widgets; the caller keeps
    // them alive for the Screen's lifetime. Supply nullptr to remove.
    void set_widget(Slot slot, Widget* widget) { m_widgets[slot] = widget; }
    Widget* widget(Slot slot) const { return m_widgets[slot]; }

    // Slot rectangles (computed live from the driver size).
    struct SlotRect {
        int x, y, w, h;
    };
    SlotRect slot_rect(Slot slot) const {
        const int w = m_driver.width();
        const int h = m_driver.height();
        switch (slot) {
            case UL: return {0, 0, w / 2, h / 2};
            case UR: return {w / 2, 0, w - w / 2, h / 2};
            case BL: return {0, h / 2, w / 2, h - h / 2};
            case BR: return {w / 2, h / 2, w - w / 2, h - h / 2};
            case FS: return {0, 0, w, h};
            default: return {0, 0, w, h};
        }
    }

    void clear(Color color = kColorBlack) {
        m_driver.fill_rect(0, 0, m_driver.width() - 1, m_driver.height() - 1, color);
    }

    // Redraw every widget in draw order, then flush the driver.
    void draw() {
        static_assert(Slot::kSlotCount == 5, "keep slot table in sync");
        for (Slot s : {UL, UR, BL, BR, FS})
            if (m_widgets[s]) m_widgets[s]->draw(m_driver);
        m_driver.flush();
    }

    // Convenience: clear + draw.
    void update(Color bg = kColorBlack) {
        clear(bg);
        draw();
    }

    void set_backlight(uint8_t level) { m_driver.set_backlight(level); }

private:
    DisplayDriver& m_driver;
    Widget* m_widgets[kSlotCount]{};
};

} // namespace pico_toolset