#pragma once

#include <Arduino.h>

// Provide a minimal tImage definition in case the exported bitmap references it
// without declaring the type. The field order matches the common LCD Image
// Converter output and is unused elsewhere.
#ifndef TIMAGE_SHIM_DEFINED
typedef struct {
  const uint16_t *data;
  uint16_t width;
  uint16_t height;
  uint8_t dataSize;
} tImage;
#define TIMAGE_SHIM_DEFINED
#endif

// Pull in the user-supplied vinyl skin. This file should define either
// `image_data_Image` or `vinyl_ui_bitmap` with 240x240 RGB565 pixels.
#include "vinyl_ui.h"

#ifndef VINYL_UI_WIDTH
#define VINYL_UI_WIDTH 240
#endif

#ifndef VINYL_UI_HEIGHT
#define VINYL_UI_HEIGHT 240
#endif

#if defined(image_data_Image)
#define VINYL_UI_BITMAP_PTR image_data_Image
#elif defined(vinyl_ui_bitmap)
#define VINYL_UI_BITMAP_PTR vinyl_ui_bitmap
#else
#error "vinyl_ui bitmap symbol not found; export image_data_Image or vinyl_ui_bitmap"
#endif

