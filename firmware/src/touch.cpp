#include "touch.h"
#include "board_cfg.h"
#include "touch_tt21100.h"
#include "touch_gt911.h"
#include <Wire.h>
#include <Arduino.h>

static touch_hw_t s_type = TOUCH_TYPE_NONE;

static bool i2c_probe(uint8_t addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission() == 0;
}

bool touch_probe(void) {
#if !BOARD_HAS_TOUCH
    s_type = TOUCH_TYPE_NONE;
    return true;
#else

#if BOARD_HAS_GT911
    // GT911 check must come first: on BOX-3 the result also selects the display driver.
    if (i2c_probe(GT911_ADDR_A) || i2c_probe(GT911_ADDR_B)) {
        s_type = TOUCH_TYPE_GT911;
        Serial.println("touch_probe: GT911");
        return true;
    }
#endif

    if (i2c_probe(TT21100_ADDR)) {
        s_type = TOUCH_TYPE_TT21100;
        Serial.println("touch_probe: TT21100");
        return true;
    }

    s_type = TOUCH_TYPE_NONE;
    Serial.println("touch_probe: no controller found");
    return false;
#endif
}

bool touch_init(void) {
    if (s_type == TOUCH_TYPE_GT911)   return gt911_init();
    if (s_type == TOUCH_TYPE_TT21100) return tt21100_init();
    return false;
}

touch_hw_t touch_hw_type(void) { return s_type; }

bool touch_read(uint16_t *x, uint16_t *y) {
    uint16_t rx, ry;

    if (s_type == TOUCH_TYPE_GT911) {
        if (!gt911_read(&rx, &ry)) return false;
        // GT911 on BOX-3: coordinates are reported in panel orientation,
        // no mirror needed based on Espressif BSP default config.
        *x = rx;
        *y = ry;
        return true;
    }

    if (s_type == TOUCH_TYPE_TT21100) {
        if (!tt21100_read(&rx, &ry)) return false;
        // TT21100 X axis is mirrored relative to the display on all BOX variants.
        *x = (LCD_WIDTH - 1) - rx;
        *y = ry;
        return true;
    }

    return false;
}
