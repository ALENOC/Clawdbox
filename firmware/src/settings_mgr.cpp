#include "settings_mgr.h"
#include <Preferences.h>

static Preferences  prefs;
static DevSettings  s_cfg;

void settings_init(void) {
    prefs.begin("devsettings", true);
    s_cfg.brightness   = prefs.getUChar ("bl",           200);
    s_cfg.standby_en   = prefs.getBool  ("standby_en",   false);
    s_cfg.standby_min  = prefs.getUShort("standby_min",  10);
    s_cfg.night_en     = prefs.getBool  ("night_en",     false);
    s_cfg.night_start  = prefs.getUChar ("night_start",  22);
    s_cfg.night_end    = prefs.getUChar ("night_end",    7);
    s_cfg.tz_offset    = (int8_t)prefs.getChar("tz_off", 0);
    prefs.end();
}

void settings_clear(void) {
    prefs.begin("devsettings", false);
    prefs.clear();
    prefs.end();
    settings_init();
}

const DevSettings* settings_get(void) { return &s_cfg; }

void settings_set_brightness(uint8_t v) {
    s_cfg.brightness = v;
    prefs.begin("devsettings", false);
    prefs.putUChar("bl", v);
    prefs.end();
}

void settings_set_standby(bool en, uint16_t min) {
    s_cfg.standby_en  = en;
    s_cfg.standby_min = min;
    prefs.begin("devsettings", false);
    prefs.putBool  ("standby_en",  en);
    prefs.putUShort("standby_min", min);
    prefs.end();
}

void settings_set_night(bool en, uint8_t start, uint8_t end) {
    s_cfg.night_en    = en;
    s_cfg.night_start = start;
    s_cfg.night_end   = end;
    prefs.begin("devsettings", false);
    prefs.putBool ("night_en",    en);
    prefs.putUChar("night_start", start);
    prefs.putUChar("night_end",   end);
    prefs.end();
}

void settings_set_tz_offset(int8_t v) {
    s_cfg.tz_offset = v;
    prefs.begin("devsettings", false);
    prefs.putChar("tz_off", v);
    prefs.end();
}
