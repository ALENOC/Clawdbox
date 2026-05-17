#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TOUCH_TYPE_NONE,
    TOUCH_TYPE_TT21100,
    TOUCH_TYPE_GT911,
} touch_hw_t;

// I2C probe to detect touch controller type. Call after Wire.begin(),
// before display init (BOX-3 needs this to select the display driver).
bool        touch_probe(void);

// Full controller init (call after display reset pulse has settled).
// Returns false if no touch hardware is present.
bool        touch_init(void);

// Detected controller type. Valid after touch_probe().
touch_hw_t  touch_hw_type(void);

// Returns true if a finger is present, writing display-corrected (x, y).
// X-axis mirror for TT21100 is applied here, not in the caller.
bool        touch_read(uint16_t *x, uint16_t *y);
