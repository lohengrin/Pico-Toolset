# boards/

This directory holds the **pico-sdk board-definition headers** the build needs
(currently [`waveshare_rp2350_pizero.h`](waveshare_rp2350_pizero.h)). Point
`PICO_BOARD_HEADER_DIRS` at it before `pico_sdk_import.cmake` runs, then set
`PICO_BOARD=waveshare_rp2350_pizero`. The other supported boards use stock pico-sdk
boards (`pico`, `pico_w`, `pico2`).

Board *documentation* (pin maps, presets, clocks, gotchas) lives in
[`docs/boards/`](../docs/boards/README.md).
