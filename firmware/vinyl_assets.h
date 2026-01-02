#pragma once

#include <Arduino.h>

// Pull in the user-supplied vinyl skin. This file should provide the raw
// RGB565 array (240x240). No compile-time symbol probing is attempted because
// variables cannot be detected by the preprocessor; we simply alias the
// expected common names.
#include "vinyl_ui.h"

#ifndef VINYL_UI_WIDTH
#define VINYL_UI_WIDTH 240
#endif

#ifndef VINYL_UI_HEIGHT
#define VINYL_UI_HEIGHT 240
#endif

// Default to the standard LCD Image Converter symbol. If a different symbol
// name is used, define VINYL_UI_BITMAP_PTR before including this header or
// add a matching extern below.
#ifndef VINYL_UI_BITMAP_PTR
extern const uint16_t image_data_Image[] PROGMEM;
#define VINYL_UI_BITMAP_PTR image_data_Image
#endif

// Optional alias for headers that export `vinyl_ui_bitmap` instead.
#ifndef VINYL_UI_BITMAP_PTR
extern const uint16_t vinyl_ui_bitmap[] PROGMEM;
#define VINYL_UI_BITMAP_PTR vinyl_ui_bitmap
#endif

