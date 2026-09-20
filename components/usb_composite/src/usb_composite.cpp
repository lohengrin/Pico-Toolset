#include "pico_toolset/usb_composite.h"

#include "tusb.h"

#include "pico/stdio/driver.h"
#include "pico/stdlib.h"
#include "pico/unique_id.h"
#include "pico/usb_reset.h"

#include <cstring>

namespace pico_toolset {

namespace {

UsbCompositeConfig g_config;
bool g_ejected = false;
bool g_media_changed = false;
uint32_t g_cached_blocks = 0; // capacity of the current medium; 0 = not read yet
char g_serial[2 * PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];

enum { ITF_CDC = 0, ITF_CDC_DATA, ITF_RESET, ITF_MSC, ITF_COUNT };
static_assert(ITF_RESET == PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF, "keep CMake's reset interface number in sync");
enum { EP_CDC_NOTIF = 0x81, EP_CDC_OUT = 0x02, EP_CDC_IN = 0x82, EP_MSC_OUT = 0x03, EP_MSC_IN = 0x83 };
enum { STR_LANG, STR_MANUF, STR_PRODUCT, STR_SERIAL, STR_CDC, STR_MSC, STR_RESET };

constexpr uint16_t kConfigTotalLen = TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_MSC_DESC_LEN + TUD_RPI_RESET_DESC_LEN;

const uint8_t kConfigDescriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_COUNT, 0, kConfigTotalLen, 0x00, 100),
    TUD_CDC_DESCRIPTOR(ITF_CDC, STR_CDC, EP_CDC_NOTIF, 8, EP_CDC_OUT, EP_CDC_IN, 64),
    // Vendor reset interface: lets picotool reboot the device into BOOTSEL
    // (or back to flash) over USB, no BOOTSEL button needed. Interface 2 to
    // match pico_stdio_usb / usb_hid's PICO_USB_RESET_MS_OS_20_DESCRIPTOR_ITF.
    TUD_RPI_RESET_DESCRIPTOR(ITF_RESET, STR_RESET),
    TUD_MSC_DESCRIPTOR(ITF_MSC, STR_MSC, EP_MSC_OUT, EP_MSC_IN, 64),
};

tusb_desc_device_t g_device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0210, // BOS descriptor (MS OS 2.0, driverless reset interface on Windows)
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0,
    .idProduct = 0,
    .bcdDevice = 0x0100,
    .iManufacturer = STR_MANUF,
    .iProduct = STR_PRODUCT,
    .iSerialNumber = STR_SERIAL,
    .bNumConfigurations = 1,
};

// stdio over CDC ------------------------------------------------------------

void cdc_out_chars(const char* buf, int len) {
    if (!tud_cdc_connected()) return;
    int sent = 0;
    absolute_time_t deadline = make_timeout_time_ms(50);
    while (sent < len) {
        sent += static_cast<int>(tud_cdc_write(buf + sent, static_cast<uint32_t>(len - sent)));
        tud_task();
        if (sent < len && time_reached(deadline)) return; // host not draining: drop
    }
}

void cdc_out_flush() {
    tud_cdc_write_flush();
    tud_task();
}

int cdc_in_chars(char* buf, int len) {
    tud_task();
    if (!tud_cdc_available()) return PICO_ERROR_NO_DATA;
    return static_cast<int>(tud_cdc_read(buf, static_cast<uint32_t>(len)));
}

stdio_driver_t g_stdio_driver = {
    .out_chars = cdc_out_chars,
    .out_flush = cdc_out_flush,
    .in_chars = cdc_in_chars,
    .set_chars_available_callback = nullptr,
    .next = nullptr,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .last_ended_with_cr = false,
    .crlf_enabled = true,
#endif
};

} // namespace

void usb_composite_init(const UsbCompositeConfig& config) {
    g_config = config;
    g_device_descriptor.idVendor = config.vid;
    g_device_descriptor.idProduct = config.pid;
    pico_get_unique_board_id_string(g_serial, sizeof(g_serial));

    tusb_init();
    if (config.install_stdio) {
        stdio_set_driver_enabled(&g_stdio_driver, true);
    }
}

void usb_composite_task() { tud_task(); }

bool usb_composite_cdc_connected() { return tud_cdc_connected(); }

void usb_composite_media_changed() {
    g_media_changed = true;
    g_ejected = false;
    g_cached_blocks = 0;
}

bool usb_composite_ejected() { return g_ejected; }

} // namespace pico_toolset

// TinyUSB device callbacks --------------------------------------------------

using pico_toolset::g_config;

extern "C" {

const uint8_t* tud_descriptor_device_cb(void) {
    return reinterpret_cast<const uint8_t*>(&pico_toolset::g_device_descriptor);
}

const uint8_t* tud_descriptor_configuration_cb(uint8_t) { return pico_toolset::kConfigDescriptor; }

const uint16_t* tud_descriptor_string_cb(uint8_t index, uint16_t) {
    static uint16_t desc[33];
    const char* str = nullptr;
    size_t chr_count;

    if (index == pico_toolset::STR_LANG) {
        desc[1] = 0x0409;
        chr_count = 1;
    } else {
        switch (index) {
            case pico_toolset::STR_MANUF: str = g_config.manufacturer; break;
            case pico_toolset::STR_PRODUCT: str = g_config.product; break;
            case pico_toolset::STR_SERIAL: str = pico_toolset::g_serial; break;
            case pico_toolset::STR_CDC: str = "Serial"; break;
            case pico_toolset::STR_MSC: str = "Mass storage"; break;
            case pico_toolset::STR_RESET: str = "Reset"; break;
            default: return nullptr;
        }
        chr_count = strlen(str);
        if (chr_count > 32) chr_count = 32;
        for (size_t i = 0; i < chr_count; ++i) desc[1 + i] = static_cast<uint8_t>(str[i]);
    }
    desc[0] = static_cast<uint16_t>((TUSB_DESC_STRING << 8) | (2 * chr_count + 2));
    return desc;
}

void tud_msc_inquiry_cb(uint8_t, uint8_t vendor_id[8], uint8_t product_id[16], uint8_t product_rev[4]) {
    memset(vendor_id, ' ', 8);
    memset(product_id, ' ', 16);
    memcpy(vendor_id, g_config.msc_vendor, strnlen(g_config.msc_vendor, 8));
    memcpy(product_id, g_config.msc_product, strnlen(g_config.msc_product, 16));
    memcpy(product_rev, "1.0 ", 4);
}

bool tud_msc_test_unit_ready_cb(uint8_t lun) {
    const auto& bd = g_config.block_device;
    if (pico_toolset::g_ejected) {
        tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00); // medium not present
        return false;
    }
    if (pico_toolset::g_media_changed) {
        pico_toolset::g_media_changed = false;
        pico_toolset::g_cached_blocks = 0;
        tud_msc_set_sense(lun, SCSI_SENSE_UNIT_ATTENTION, 0x28, 0x00); // not ready to ready change: media may have changed
        return false;
    }
    if (bd.ready && bd.ready(bd.ctx)) return true;
    pico_toolset::g_cached_blocks = 0;
    tud_msc_set_sense(lun, SCSI_SENSE_NOT_READY, 0x3A, 0x00); // medium not present
    return false;
}

void tud_msc_capacity_cb(uint8_t, uint32_t* block_count, uint16_t* block_size) {
    const auto& bd = g_config.block_device;
    *block_size = 512;
    *block_count = 0; // 0 makes TinyUSB answer "medium not present"
    if (pico_toolset::g_ejected || !bd.block_count || !bd.ready || !bd.ready(bd.ctx)) return;

    // The capacity is asked for constantly by some hosts and costs a card
    // command each time: read it once per medium, never cache a failure.
    if (pico_toolset::g_cached_blocks == 0) pico_toolset::g_cached_blocks = bd.block_count(bd.ctx);
    *block_count = pico_toolset::g_cached_blocks;
}

// Eject ("safely remove"): flush first, then report no medium. A host "load"
// (start + load_eject) brings it back and tells the host it may have changed.
bool tud_msc_start_stop_cb(uint8_t, uint8_t, bool start, bool load_eject) {
    const auto& bd = g_config.block_device;
    if (load_eject && !start) {
        if (bd.sync) bd.sync(bd.ctx);
        pico_toolset::g_ejected = true;
    } else if (load_eject && start) {
        pico_toolset::usb_composite_media_changed();
    }
    return true;
}

int32_t tud_msc_read10_cb(uint8_t, uint32_t lba, uint32_t offset, void* buffer, uint32_t bufsize) {
    const auto& bd = g_config.block_device;
    if (offset != 0 || (bufsize % 512) != 0 || !bd.read || pico_toolset::g_ejected) return -1;
    const uint32_t count = bufsize / 512;
    if (pico_toolset::g_cached_blocks != 0 && lba + count > pico_toolset::g_cached_blocks) return -1; // past the end
    return bd.read(bd.ctx, lba, static_cast<uint8_t*>(buffer), count) ? static_cast<int32_t>(bufsize) : -1;
}

int32_t tud_msc_write10_cb(uint8_t, uint32_t lba, uint32_t offset, uint8_t* buffer, uint32_t bufsize) {
    const auto& bd = g_config.block_device;
    if (offset != 0 || (bufsize % 512) != 0 || !bd.write || pico_toolset::g_ejected) return -1;
    const uint32_t count = bufsize / 512;
    if (pico_toolset::g_cached_blocks != 0 && lba + count > pico_toolset::g_cached_blocks) return -1; // past the end
    return bd.write(bd.ctx, lba, buffer, count) ? static_cast<int32_t>(bufsize) : -1;
}

// Reported in MODE SENSE and enforced on WRITE(10): TinyUSB answers DATA
// PROTECT (write protected) when this is false.
bool tud_msc_is_writable_cb(uint8_t) {
    const auto& bd = g_config.block_device;
    return !pico_toolset::g_ejected && (!bd.writable || bd.writable(bd.ctx));
}

// Commands TinyUSB doesn't handle itself. SYNCHRONIZE CACHE is sent by hosts
// after writes; failing it makes them report the write as failed and remount
// read-only, so it succeeds -- but only once the backend really flushed.
int32_t tud_msc_scsi_cb(uint8_t lun, const uint8_t scsi_cmd[16], void*, uint16_t) {
    if (scsi_cmd[0] == 0x35 /* SYNCHRONIZE CACHE (10) */) {
        const auto& bd = g_config.block_device;
        if (!bd.sync || bd.sync(bd.ctx)) return 0;
        tud_msc_set_sense(lun, SCSI_SENSE_MEDIUM_ERROR, 0x0C, 0x00); // write error
        return -1;
    }
    tud_msc_set_sense(lun, SCSI_SENSE_ILLEGAL_REQUEST, 0x20, 0x00);
    return -1;
}

} // extern "C"
