// Replaces Waveshare's DEV_Config.h for the vendored GUI_Paint/fonts: only the
// integer typedefs they use. The hardware layer (pins/SPI) lives in the
// config-driven Epd2in13V4 class instead.
#ifndef _DEV_CONFIG_H_
#define _DEV_CONFIG_H_

#include <stdint.h>
#include <stdio.h>

#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

#endif
