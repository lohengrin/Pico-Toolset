#include "pico_toolset/sdcard.h"

#include "ff.h"
#include "hardware/pio.h"
#include "tf_card.h"

#include <algorithm>
#include <cctype>

namespace pico_toolset {

namespace {

// pico_fatfs requires this to have static storage duration for the lifetime
// of the mount (FatFs keeps a pointer to it, it doesn't copy the struct).
// Also enforces this driver's "only one SdCard mounted at a time" contract.
FATFS g_fatfs;

std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
    return s;
}

std::string extension_of(const std::string& filename) {
    auto dot = filename.find_last_of('.');
    if (dot == std::string::npos) return "";
    return to_lower(filename.substr(dot + 1));
}

} // namespace

bool SdCard::init(const SdCardConfig& config) {
    if (config.gpio_base >= 0) {
        pio_set_gpio_base(config.pio, static_cast<uint>(config.gpio_base));
    }

    pico_fatfs_spi_config_t spi_config{
        config.spi_instance,
        config.clk_slow_hz,
        config.clk_fast_hz,
        config.pin_miso,
        config.pin_cs,
        config.pin_sck,
        config.pin_mosi,
        config.pullup,
    };
    m_used_native_spi = pico_fatfs_set_config(&spi_config);
    if (!m_used_native_spi) {
        pico_fatfs_config_spi_pio(config.pio, config.sm);
    }

    m_last_mount_result = f_mount(&g_fatfs, "", 1);
    m_mounted = (m_last_mount_result == FR_OK);
    return m_mounted;
}

std::vector<std::string> SdCard::list_files(const std::vector<std::string>& extensions) const {
    std::vector<std::string> result;
    if (!m_mounted) return result;

    DIR dir;
    if (f_opendir(&dir, "") != FR_OK) return result;

    FILINFO info;
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
        if (info.fattrib & AM_DIR) continue;
        std::string name(info.fname);
        std::string ext = extension_of(name);
        if (std::find(extensions.begin(), extensions.end(), ext) != extensions.end()) {
            result.push_back(name);
        }
    }
    f_closedir(&dir);
    return result;
}

bool SdCard::list_dir(const std::string& path, const std::vector<std::string>& extensions,
                      std::vector<FileInfo>& out, size_t max_entries, bool* truncated, bool skip_hidden) const {
    out.clear();
    if (truncated) *truncated = false;
    if (!m_mounted) return false;

    DIR dir;
    if (f_opendir(&dir, path.c_str()) != FR_OK) return false;

    FILINFO info;
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
        if (skip_hidden && ((info.fattrib & (AM_HID | AM_SYS)) || info.fname[0] == '.')) continue;
        const bool is_dir = (info.fattrib & AM_DIR) != 0;
        std::string name(info.fname);
        if (!is_dir && std::find(extensions.begin(), extensions.end(), extension_of(name)) == extensions.end()) continue;
        if (max_entries != 0 && out.size() >= max_entries) {
            if (truncated) *truncated = true;
            break;
        }
        out.push_back(FileInfo{std::move(name), is_dir ? 0u : static_cast<uint32_t>(info.fsize), is_dir});
    }
    f_closedir(&dir);
    return true;
}

std::vector<SdCard::FileInfo> SdCard::list_file_info(const std::vector<std::string>& extensions, size_t max_entries,
                                                     bool* truncated, bool skip_hidden) const {
    std::vector<FileInfo> result;
    if (truncated) *truncated = false;
    if (!m_mounted) return result;

    DIR dir;
    if (f_opendir(&dir, "") != FR_OK) return result;

    FILINFO info;
    while (f_readdir(&dir, &info) == FR_OK && info.fname[0] != '\0') {
        if (info.fattrib & AM_DIR) continue;
        if (skip_hidden && ((info.fattrib & (AM_HID | AM_SYS)) || info.fname[0] == '.')) continue;
        std::string name(info.fname);
        if (std::find(extensions.begin(), extensions.end(), extension_of(name)) == extensions.end()) continue;
        if (max_entries != 0 && result.size() >= max_entries) {
            if (truncated) *truncated = true;
            break;
        }
        result.push_back(FileInfo{std::move(name), static_cast<uint32_t>(info.fsize), false});
    }
    f_closedir(&dir);
    return result;
}

std::vector<uint8_t> SdCard::read_file(const std::string& filename) const {
    std::vector<uint8_t> data;
    if (!m_mounted) return data;

    FIL file;
    if (f_open(&file, filename.c_str(), FA_READ | FA_OPEN_EXISTING) != FR_OK) return data;

    FSIZE_t size = f_size(&file);
    data.resize(size);
    UINT bytes_read = 0;
    f_read(&file, data.data(), static_cast<UINT>(size), &bytes_read);
    data.resize(bytes_read);

    f_close(&file);
    return data;
}

std::pmr::vector<uint8_t> SdCard::read_file_pmr(const std::string& filename, std::pmr::memory_resource* resource) const {
    std::pmr::vector<uint8_t> data(resource);
    if (!m_mounted) return data;

    FIL file;
    if (f_open(&file, filename.c_str(), FA_READ | FA_OPEN_EXISTING) != FR_OK) return data;

    FSIZE_t size = f_size(&file);
    data.resize(size);
    UINT bytes_read = 0;
    f_read(&file, data.data(), static_cast<UINT>(size), &bytes_read);
    data.resize(bytes_read);

    f_close(&file);
    return data;
}

} // namespace pico_toolset
