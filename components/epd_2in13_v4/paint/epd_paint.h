#pragma once

// Waveshare's GUI_Paint/fonts headers have no extern "C" guards; fonts.h does.
#ifdef __cplusplus
extern "C" {
#endif

#include "GUI_Paint.h"
#include "fonts.h"

#ifdef __cplusplus
}
#endif
