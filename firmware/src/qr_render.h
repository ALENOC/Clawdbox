#pragma once
#include <Arduino.h>
#include <lvgl.h>

// Encode `text` as QR (ricmoo/QRCode), render to an RGB565 buffer in PSRAM,
// expose as an lv_image_dsc_t. Picks the smallest version (10..20) that fits
// the text at ECC_LOW. Module size is chosen so the final image is <= target_px.
// Returns false on encode failure or OOM.
bool qr_render(const char* text, int target_px);

const lv_image_dsc_t* qr_get_image(void);
int  qr_get_size_px(void);
void qr_free(void);
