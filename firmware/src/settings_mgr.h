#pragma once
#include <stdint.h>
#include <stdbool.h>

struct DevSettings {
    uint8_t  brightness;    // 0-255
    bool     standby_en;
    uint16_t standby_min;   // minutes until standby
    bool     night_en;      // restrict standby to night window
    uint8_t  night_start;   // UTC hour 0-23
    uint8_t  night_end;     // UTC hour 0-23
};

void               settings_init(void);
const DevSettings* settings_get(void);
void               settings_set_brightness(uint8_t v);
void               settings_set_standby(bool en, uint16_t min);
void               settings_set_night(bool en, uint8_t start, uint8_t end);
