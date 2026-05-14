#include "power.h"

// ESP32-S3-BOX has no PMU/battery. Neutral stubs.
// ui_update_battery() interprets pct < 0 as "no battery, hide icon".
void power_init(void)  {}
void power_tick(void)  {}
int  power_battery_pct(void) { return -1; }
bool power_is_charging(void) { return false; }
bool power_pwr_pressed(void) { return false; }
