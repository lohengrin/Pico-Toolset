#pragma once

#include <cstdint>

#include "hardware/spi.h"

#include "pico_toolset/touch_panel.h"

namespace pico_toolset {

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
    uint8_t     pin_irq;                // Active-low PENIRQ, polled (not IRQ-driven)
    uint32_t    touch_freq_hz = 2'000'000; // Touch-appropriate SPI clock
};

// Raw, uncalibrated reader for an XPT2046-compatible resistive touch
// controller.
//
// WIRE PROTOCOL: standard XPT2046 command/response framing -- an 8-bit
// control byte (channel select + mode bits) followed by two bytes of
// response, of which the 12-bit ADC result sits in bits 14-3 (bit 15 is a
// leading don't-care, the low 3 bits are padding).
//
// SHARED-BUS CONTRACT: this driver does not call spi_init() -- it shares an
// SPI instance a display driver on the same bus (e.g. pico_toolset::Ili9486)
// already brought up, taking over only its own CS pin and the touch-specific
// baud rate for the duration of each read(). Callers must not call read()
// while that display driver has an open set_window()/write_pixels()
// sequence -- the two devices share CS-gated access to one bus.
//
// Returns raw, uncalibrated 12-bit ADC readings -- see xpt2046_calibration.h
// for mapping them into pixel space.
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

    // Configures the CS/IRQ GPIOs. Does not touch the SPI peripheral itself
    // -- config.spi_instance must already be spi_init()'d by whatever else
    // owns the bus (pass the same spi_inst_t* that driver was configured
    // with).
    void init(const Xpt2046Config& config);

    // Polls PENIRQ first (cheap, avoids an SPI transaction on every idle
    // poll); if pressed, reads raw X/Y over SPI at the configured
    // touch-appropriate baud rate.
    [[nodiscard]] RawSample read() override;

private:
    [[nodiscard]] uint16_t read_channel(uint8_t control_byte);

    spi_inst_t* m_spi = spi1;
    uint8_t     m_pin_cs = 7;
    uint8_t     m_pin_irq = 17;
    uint32_t    m_touch_baud = 2'000'000;
};

} // namespace pico_toolset
