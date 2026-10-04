#include <cstdint>
#include <cstdio>

#include "pico/stdlib.h"
#include "pico_toolset/epd_2in13_v4.h"
#include "pico_toolset/epd_2in13_v4_configs.h"
#include "epd_paint.h"

using namespace pico_toolset;

static uint8_t fb[Epd2in13V4::kFramebufferSize];

int main() {
    stdio_init_all();

    Epd2in13V4 epd;
    if (!epd.init(configs::epd_2in13_v4::kWavesharePicoEpaper213)) {
        std::printf("epd init failed\n");
        for (;;) tight_loop_contents();
    }

    // Landscape canvas (250x122) over the native portrait buffer.
    Paint_NewImage(fb, Epd2in13V4::kWidth, Epd2in13V4::kHeight, 90, WHITE);
    Paint_SelectImage(fb);

    // Paint_DrawString_EN swaps its colour arguments: effective order is
    // (cell background, glyph colour); a WHITE background draws glyphs only.
    for (int n = 0;; n++) {
        Paint_Clear(WHITE);
        Paint_DrawRectangle(0, 0, 249, 121, BLACK, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
        char text[32];
        std::snprintf(text, sizeof text, "update %d", n);
        Paint_DrawString_EN(10, 50, text, &Font16, WHITE, BLACK);
        epd.update(fb);

        if (n % 100 == 99) epd.clear_screen();
        sleep_ms(10000);
    }
}
