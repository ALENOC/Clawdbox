#pragma once
#include "data.h"

void poll_init(void);
void poll_tick(UsageData* out, bool* updated);   // call from loop()
bool poll_force_now(UsageData* out);             // synchronous one-shot
void poll_reset_timer(void);                     // force poll on next tick (e.g. after standby wake)
