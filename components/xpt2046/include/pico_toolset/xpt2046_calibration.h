#pragma once

#include <cstdint>

namespace pico_toolset {

// Raw-ADC-to-pixel-space linear calibration for Xpt2046Touch. A resistive
// touch panel's raw 12-bit ADC range rarely lines up with the display's own
// pixel axes -- it can be rotated relative to the panel (swap_axes) and/or
// wired so either axis reads high-to-low instead of low-to-high (min may
// legitimately exceed max; touch_calibration_linear_map() below handles
// that with no special-casing).
//
// The defaults here are one specific Waveshare 3.5" RPi LCD (A) unit's
// measured corners -- a STARTING POINT, not a universal constant: every
// physical panel needs its own two-corner measurement (touch each corner,
// read the raw values back, e.g. via a small bring-up print in read()'s
// caller) and its own Xpt2046Calibration instance built from those.
struct Xpt2046Calibration {
    // Which raw ADC channel (raw_x/raw_y) drives which display axis.
    bool swap_axes = true;

    // Raw ADC range mapped to the display's pixel range on each axis
    // (h = horizontal 0..width-1, v = vertical 0..height-1). min may exceed
    // max when the panel's wiring runs opposite to the intended screen
    // direction -- touch_calibration_linear_map() handles that directly.
    //
    // Measured on real hardware (480x320 panel) from two opposite corners:
    //   top-left     raw_x=3748 raw_y=280
    //   bottom-right raw_x=274  raw_y=3908
    // so, with swap_axes: horizontal comes from raw_y (280 -> 3908
    // left-to-right), vertical from raw_x (3748 -> 274 top-to-bottom,
    // decreasing, hence min > max).
    uint16_t raw_h_min = 280, raw_h_max = 3908;
    uint16_t raw_v_min = 3748, raw_v_max = 274;
};

// Linear map from a raw ADC sample to pixel space, given the panel's
// measured raw range and the target pixel range. Handles in_min > in_max
// (an axis wired to read high-to-low) with no special-casing -- `t` simply
// runs from 0 at in_min to 1 at in_max regardless of which is numerically
// larger.
inline double touch_calibration_linear_map(double v, double in_min, double in_max, double out_min, double out_max) {
    double t = (v - in_min) / (in_max - in_min);
    return out_min + t * (out_max - out_min);
}

} // namespace pico_toolset
