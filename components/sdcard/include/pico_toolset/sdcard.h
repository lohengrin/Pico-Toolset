#pragma once

#include <cstdint>
#include <memory_resource>
#include <string>
#include <vector>

#include "hardware/pio.h"
#include "hardware/spi.h"

namespace pico_toolset {

// Configuration for a uSD card over FatFs R0.15 (elehobica/pico_fatfs), in
// either native-hardware SPI or PIO-bit-banged SPI mode. Many carrier
// boards wire the card's native 4-bit SDIO interface (CS=DAT3, MOSI=CMD,
// MISO=DAT0, SCK=CLK) to pins with no hardware-SPI alternate function --
// set spi_instance = nullptr to request PIO-bit-banged SPI in that case
// (pico_fatfs_set_config() reports back which mode it actually configured,
// see SdCard::used_native_spi()).
// No field below has a working default -- pins/SPI-vs-PIO mode/PIO block are
// board wiring, not driver behavior -- see sdcard_configs.h for known-good
// per-board presets (e.g. configs::sdcard::kWaveshareRp2350PiZero), or fill in
// every field yourself for a board without one yet.
struct SdCardConfig {
    spi_inst_t* spi_instance = nullptr; // nullptr = PIO-bit-banged SPI -- must be set either way
    uint8_t     pin_miso;
    uint8_t     pin_cs;
    uint8_t     pin_sck;
    uint8_t     pin_mosi;
    bool        pullup   = true;        // MISO/MOSI pins only

    uint32_t clk_slow_hz = 100'000;     // card-init clock (pico_fatfs' own CLK_SLOW_DEFAULT)
    uint32_t clk_fast_hz = 10'000'000;  // steady-state clock -- kept well below pico_fatfs'
                                         // 50MHz CLK_FAST_DEFAULT, which assumes a clean PCB
                                         // trace; a high SPI clock over jumper wires causes
                                         // intermittent corruption that reads as "no card"

    PIO  pio; // PIO block for bit-banged SPI (ignored if spi_instance is set) -- must be set
    uint sm = 0; // PIO state machine index within that block

    // Whether/how to call pio_set_gpio_base() before mounting. RP2350B's PIO
    // blocks each see only one 32-pin-wide addressing window (default
    // 0-31); if your configured pins straddle the 32-pin boundary (e.g.
    // SCK/MOSI in 16-31, MISO in 32-47), the default window can't reach all
    // of them and pico_fatfs's PIO program gets silently wrong
    // EXECCTRL/PINCTRL rather than an error (confirmed on real hardware: an
    // "SD card times out forever" symptom, not a clean failure). -1 (the
    // default) leaves pico_fatfs' own gpio_base alone -- set this to a
    // value that widens the window to cover every configured pin (e.g. 16
    // to cover 16-47) when needed. Also requires PICO_PIO_USE_GPIO_BASE=1
    // to reach hardware_pio's own compilation -- see cmake/pico_fatfs.cmake,
    // which sets this automatically for any consumer of the `pico_fatfs`
    // target.
    int gpio_base = -1;
};

// uSD card access via FatFs R0.15 (pico_fatfs). Config-driven -- no
// hardcoded pins, clock, or PIO assignment; every one of a board's uSD
// wiring quirks (native vs bit-banged SPI, a >=32 pin straddling the PIO
// addressing window) is a config field, not something baked into this
// driver.
//
// Only one SdCard should be mounted at a time in a given program -- FatFs
// itself is a single-volume-per-FATFS-struct design, and this driver keeps
// that struct's storage inside its own translation unit (pico_fatfs keeps a
// pointer to it for the mount's lifetime, so it can't be a stack/heap
// temporary).
//
// Derived from TOM6809's real-hardware-validated driver, unifying its two
// board-specific variants into one config-driven implementation (MIT).
class SdCard {
public:
    // Configures the SD SPI pins/PIO and mounts the card. Returns false on
    // failure (e.g. no card) -- callers should treat that as "SD support
    // disabled for this session", not fatal: a board with no card inserted
    // is a normal, expected state.
    bool init(const SdCardConfig& config);

    // Lists root-directory files whose extension (case-insensitive, no
    // dot) is one of `extensions`. Root only -- no subdirectory traversal.
    [[nodiscard]] std::vector<std::string> list_files(const std::vector<std::string>& extensions) const;

    // Reads a whole file from the SD root by name into memory. Returns an
    // empty vector on failure (file missing, read error, card not mounted).
    [[nodiscard]] std::vector<uint8_t> read_file(const std::string& filename) const;

    // Like read_file(), but reads into a buffer backed by `resource` -- for
    // files too large to comfortably fit an RP2040/RP2350's on-chip SRAM
    // (e.g. a caller-supplied PSRAM resource). resource defaults to the
    // global heap, reproducing read_file() exactly.
    [[nodiscard]] std::pmr::vector<uint8_t> read_file_pmr(const std::string& filename,
                                                            std::pmr::memory_resource* resource
                                                            = std::pmr::get_default_resource()) const;

    [[nodiscard]] bool is_mounted() const { return m_mounted; }

    // Raw FatFs FRESULT from init()'s f_mount() (0 = FR_OK). Useful
    // on-screen for diagnosis: FR_DISK_ERR/FR_NOT_READY (1/3) usually mean
    // a wiring/SPI problem, FR_NO_FILESYSTEM (13) means the card responds
    // but isn't a valid FAT volume.
    [[nodiscard]] int last_mount_result() const { return m_last_mount_result; }

    // True if pico_fatfs configured real hardware SPI instead of PIO-SPI
    // (i.e. config.spi_instance was non-null and landed on a real
    // alternate-function pin set) -- mainly a diagnostic.
    [[nodiscard]] bool used_native_spi() const { return m_used_native_spi; }

private:
    bool m_mounted = false;
    int  m_last_mount_result = -1;
    bool m_used_native_spi = false;
};

} // namespace pico_toolset
