#pragma once

#include <cstdint>

#include "hardware/spi.h"

#include "pico_toolset/touch_panel.h"

namespace pico_toolset {

// Control-byte fields, per the XPT2046 datasheet's "Order of the Control
// Bits in the Control Byte" (Table 6): S(7) A2-A0(6-4) MODE(3) SER/DFR(2)
// PD1-PD0(1-0). These enums name every legal value of each field so control
// bytes are built from the spec's own vocabulary instead of hand-picked
// magic numbers.

// MODE bit: resolution of the next conversion (datasheet "Digital
// Interface" section). 8-bit mode completes 4 clock cycles earlier and can
// run its clock up to 50% faster (datasheet "8-Bit Conversion") at the cost
// of precision -- pick it for a faster/cheaper poll when only coarse
// pressure/position data is needed.
enum class Xpt2046Resolution : uint8_t {
    Bits12 = 0,
    Bits8 = 1,
};

// SER/DFR bit: differential (ratiometric, preferred for touch position --
// datasheet "SER/DFR" paragraph) vs single-ended (required for
// Battery/Aux/Temp0/Temp1 -- "the differential mode can only be used for
// X-Position, Y-Position, and Pressure-Touch measurements"). read_channel()
// enforces this automatically; Xpt2046Config::touch_reference only applies
// to X/Y/Z1/Z2.
enum class Xpt2046Reference : uint8_t {
    Differential = 0,
    SingleEnded = 1,
};

// PD1-PD0 bits: power-down/reference/PENIRQ state after a conversion
// (datasheet Table 8, "Power-Down and Internal Reference Selection").
// PENIRQ (when wired -- see Xpt2046Config::pin_irq) is enabled only by the
// two values below whose name doesn't mention "AdcOn"/"AlwaysPowered".
enum class Xpt2046PowerMode : uint8_t {
    PowerDownBetweenConversions = 0b00, // PENIRQ enabled; safe default, no wake-up delay needed
    ReferenceOffAdcOn = 0b01,           // PENIRQ disabled; external VREF required
    ReferenceOnAdcOff = 0b10,           // PENIRQ enabled; primes the internal reference
    AlwaysPowered = 0b11,               // PENIRQ disabled; internal reference + ADC both stay on
};

// A2-A0 bits: input multiplexer channel select (datasheet Table 4/Figure 5,
// and the general-purpose channel list in "Digital Interface"). Battery/
// Aux/Temp0/Temp1 need Xpt2046Reference::SingleEnded and, for the internal
// 2.5V reference to drive them (no external precision VREF wired), a
// Xpt2046PowerMode with the reference on (ReferenceOnAdcOff or
// AlwaysPowered) -- see read_channel()'s doc comment.
enum class Xpt2046Channel : uint8_t {
    Temp0 = 0b000,   // Ambient temperature, single calibration point (datasheet "Temperature Measurement")
    Y = 0b001,       // Y-position (touch)
    Battery = 0b010, // VBAT/4 (datasheet "Battery Measurement")
    Z1 = 0b011,      // Touch-pressure channel 1
    Z2 = 0b100,      // Touch-pressure channel 2
    X = 0b101,       // X-position (touch)
    Aux = 0b110,     // Auxiliary analog input (IN pin)
    Temp1 = 0b111,   // Ambient temperature, second point for the ratiometric two-measurement method
};

// Configuration for an XPT2046-compatible resistive touch controller sharing
// an SPI bus with a display panel (e.g. pico_toolset::Ili9486 on the
// Waveshare-style 3.5" RPi LCD family, where the panel is write-only and
// MISO belongs to the touch controller alone). spi_instance must match
// whichever SPI peripheral the display driver already initialized (this
// driver never calls spi_init() itself -- see the class doc comment).
//
// No field below has a working default -- see xpt2046_configs.h for
// known-good board+panel presets (e.g. configs::xpt2046::kWaveshareRp2350PiZero), or
// fill in every field yourself for a board without one yet. spi_instance in
// particular should usually be set from the co-located display driver's own
// spi() accessor (e.g. lcd.spi()) rather than copied from a preset, so the
// two drivers always agree on which bus they share.
struct Xpt2046Config {
    spi_inst_t* spi_instance = nullptr; // SPI peripheral (already initialized elsewhere) -- must be set
    uint8_t     pin_cs;                 // Touch controller's own Chip Select
    uint8_t     pin_irq;                // Active-low PENIRQ. 255 = not wired/not used -- see read()'s doc comment
    uint32_t    touch_freq_hz = 2'000'000; // Touch-appropriate SPI clock

    // Z1/Z2 pressure threshold above which read() reports a press -- see
    // read()'s doc comment for the Z = (4095-Z2)+Z1 formula. 300 is the
    // commonly-used default across other XPT2046 drivers; tune per panel if
    // touches are missed (raise) or ghost-register when idle (lower).
    // Compared against the 0-4095-normalized scale regardless of
    // `resolution` (see read_channel()'s doc comment).
    uint16_t    pressure_threshold = 300;

    // Resolution/power-mode used for every conversion this driver performs
    // (read(), read_channel(), and the raw-channel convenience wrappers).
    // Defaults match this driver's original, real-hardware-validated
    // behavior: full 12-bit precision, power-down between conversions
    // (lowest idle power, no PENIRQ wake-up delay).
    Xpt2046Resolution resolution = Xpt2046Resolution::Bits12;
    Xpt2046PowerMode  power_mode = Xpt2046PowerMode::PowerDownBetweenConversions;
    // Reference mode for the X/Y/Z1/Z2 touch-position channels specifically
    // -- the datasheet recommends Differential (ratiometric) for these, as
    // it cancels the touch-panel-driver switches' on-resistance error (see
    // "Simplified Diagram of Differential Reference"). Battery/Aux/Temp0/
    // Temp1 always use SingleEnded regardless of this setting -- the chip
    // requires it (see read_channel()'s doc comment) -- so this field has
    // no effect on those.
    Xpt2046Reference  touch_reference = Xpt2046Reference::Differential;
};

// Reader for an XPT2046-compatible resistive touch controller + auxiliary
// ADC (battery/temperature/aux-input), built from the full XPT2046
// datasheet control-byte spec (see the Xpt2046Resolution/Reference/
// PowerMode/Channel enums above) rather than a handful of hardcoded control
// bytes.
//
// WIRE PROTOCOL: standard XPT2046 command/response framing -- an 8-bit
// control byte (channel select + mode bits) followed by two bytes of
// response, of which the ADC result sits in bits 14-3 of a 12-bit
// conversion (bit 15 is a leading don't-care, the low 3 bits are padding)
// or bits 14-7 of an 8-bit conversion (Xpt2046Resolution::Bits8) -- 8-bit
// results are normalized onto the same 0-4095 scale as 12-bit ones (left-
// shifted by 4) so callers don't need resolution-specific scaling code.
//
// SHARED-BUS CONTRACT: this driver does not call spi_init() -- it shares an
// SPI instance a display driver on the same bus (e.g. pico_toolset::Ili9486)
// already brought up, taking over only its own CS pin and the touch-specific
// baud rate for the duration of each read(). Callers must not call read()
// while that display driver has an open set_window()/write_pixels()
// sequence -- the two devices share CS-gated access to one bus.
//
// Derived from a real-hardware-validated driver (MIT).
//
// Implements TouchPanel (touch_panel.h) so callers that only need a raw
// touch reading can hold a TouchPanel& instead of a concrete
// Xpt2046Touch& and swap touch controllers without changing call sites.
class Xpt2046Touch : public TouchPanel {
public:
    // Alias kept for source compatibility with existing callers that name
    // Xpt2046Touch::RawSample directly -- identical to the shared
    // pico_toolset::TouchSample (touch_panel.h).
    using RawSample = TouchSample;

    // Configures the CS/IRQ GPIOs and the resolution/power-mode/reference
    // settings from `config`. Does not touch the SPI peripheral itself --
    // config.spi_instance must already be spi_init()'d by whatever else
    // owns the bus (pass the same spi_inst_t* that driver was configured
    // with).
    void init(const Xpt2046Config& config);

    // Determines touch state from the Z1/Z2 PRESSURE channels
    // (Z = (4095-Z2)+Z1, thresholded against config.pressure_threshold),
    // NOT by polling PENIRQ -- some XPT2046/ADS7846 board designs leave
    // PENIRQ unconnected or non-functional (confirmed on real hardware,
    // 2026-09: a SunFounder 3.5" panel whose PENIRQ line never toggled
    // despite the touch layer itself working over SPI), so gating on it is
    // not a safe default. This is the datasheet's own "commonly used"
    // approximate pressure test, not the exact touch-resistance formulas
    // (3)/(4) in the "Pressure Measurement" section -- those need the
    // panel's own X-plate/Y-plate resistance, which is calibration data
    // this generic driver doesn't have. Costs 2 extra SPI conversions per
    // idle poll versus a PENIRQ pre-check, negligible at typical UI poll
    // rates. Reads X/Y (2 more conversions) only when the pressure
    // threshold is exceeded. Uses config.touch_reference for all four
    // channels.
    [[nodiscard]] RawSample read() override;

    // Generic single-channel conversion, for any of the eight channels the
    // XPT2046 supports (see Xpt2046Channel) -- not just touch position.
    // Uses this driver's configured resolution/power-mode; the reference
    // mode is forced to SingleEnded for every channel except X/Y/Z1/Z2
    // (which use config.touch_reference) -- the datasheet requires
    // single-ended for Battery/Aux/Temp0/Temp1 ("the differential mode can
    // only be used for X-Position, Y-Position, and Pressure-Touch
    // measurements"). Result is a raw ADC code normalized to 0-4095
    // regardless of `resolution` -- turning it into a physical unit (volts,
    // °C) needs the board's actual VREF wiring (external precision
    // reference vs. this chip's internal 2.5V one), which is board-specific
    // and NOT assumed here; see the datasheet's "Battery Measurement" and
    // "Temperature Measurement" sections for the conversion math once that's
    // known. For Battery/Aux/Temp0/Temp1, config.power_mode must leave the
    // reference on (ReferenceOnAdcOff or AlwaysPowered) unless VREF is
    // driven externally.
    [[nodiscard]] uint16_t read_channel(Xpt2046Channel channel);

private:
    [[nodiscard]] uint8_t build_control_byte(Xpt2046Channel channel, Xpt2046Reference reference) const;
    // Sends `control_byte` and returns the normalized (0-4095) result. CS
    // is the caller's responsibility (read() holds it low across several
    // conversions in one burst; read_channel() manages it per-call).
    [[nodiscard]] uint16_t convert(uint8_t control_byte);

    spi_inst_t* m_spi = spi1;
    uint8_t     m_pin_cs = 7;
    uint8_t     m_pin_irq = 17;
    uint32_t    m_touch_baud = 2'000'000;
    uint16_t    m_pressure_threshold = 300;
    Xpt2046Resolution m_resolution = Xpt2046Resolution::Bits12;
    Xpt2046PowerMode  m_power_mode = Xpt2046PowerMode::PowerDownBetweenConversions;
    Xpt2046Reference  m_touch_reference = Xpt2046Reference::Differential;
};

} // namespace pico_toolset
