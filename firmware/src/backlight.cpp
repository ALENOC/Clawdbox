#include "backlight.h"
#include "board_cfg.h"
#include <Arduino.h>

#define BL_FREQ 1000
#define BL_BITS 8

static uint8_t s_brightness = 200;
static bool    s_standby    = false;

void bl_init(uint8_t initial_brightness) {
    s_brightness = initial_brightness;
    ledcAttach(LCD_BL, BL_FREQ, BL_BITS);
    ledcWrite(LCD_BL, 0);  // stays dark until bl_on() after first frame
}

void bl_on(void) {
    s_standby = false;
    ledcWrite(LCD_BL, s_brightness);
}

void bl_set(uint8_t val) {
    s_brightness = val;
    if (!s_standby) ledcWrite(LCD_BL, val);
}

uint8_t bl_get(void) { return s_brightness; }

void bl_standby(void) {
    if (s_standby) return;
    s_standby = true;
    ledcWrite(LCD_BL, 0);  // off
}

void bl_wake(void) {
    if (!s_standby) return;
    s_standby = false;
    ledcWrite(LCD_BL, s_brightness);
}

bool bl_is_standby(void) { return s_standby; }
