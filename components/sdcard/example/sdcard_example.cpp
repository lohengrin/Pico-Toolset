// SD card example (doubles as an integration test).
// Mounts a card over PIO-bit-banged SPI and lists .txt files at the root.
#include "pico_toolset/sdcard.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::SdCard;
using pico_toolset::SdCardConfig;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    SdCardConfig cfg;
    // Adapt pins/PIO here for your board -- these match TOM6809's "Pico DV"
    // custom carrier board.
    cfg.spi_instance = nullptr; // PIO-bit-banged SPI (native SPI not wired on this board)
    cfg.pin_miso = 19;
    cfg.pin_cs   = 22;
    cfg.pin_sck  = 5;
    cfg.pin_mosi = 18;
    cfg.pio = pio1;
    cfg.sm = 0;
    // cfg.gpio_base only needs setting if a configured pin is >= 32 -- see
    // the config struct's own doc comment.

    SdCard sd;
    if (!sd.init(cfg)) {
        printf("SD mount failed (FRESULT=%d)\n", sd.last_mount_result());
        return 1;
    }
    printf("SD mounted (native SPI: %s)\n", sd.used_native_spi() ? "yes" : "no");

    for (const auto& name : sd.list_files({"txt"})) {
        printf("  %s\n", name.c_str());
    }

    while (true) {
        tight_loop_contents();
    }
}
