// SD card example (doubles as an integration test).
// Mounts a card over PIO-bit-banged SPI and lists .txt files at the root.
#include "pico_toolset/sdcard.h"
#include "pico_toolset/sdcard_configs.h"
#include "pico/stdlib.h"

#include <cstdio>

using pico_toolset::SdCard;

int main() {
    stdio_init_all();
    sleep_ms(2000);

    // On a different board, pick a different preset (see sdcard_configs.h)
    // or copy this one and change only the fields that differ.
    SdCard sd;
    if (!sd.init(pico_toolset::configs::sdcard::kPicoDvCarrier)) {
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
