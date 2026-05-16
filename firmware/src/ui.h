#pragma once
#include "data.h"
#include "net.h"

enum screen_t {
    SCREEN_SPLASH,
    SCREEN_USAGE,
    SCREEN_NETWORK,
    SCREEN_SETTINGS,
    SCREEN_PAIR,
    SCREEN_COUNT,
};

void ui_init(void);
void ui_update(const UsageData* data);
void ui_tick_anim(void);
void ui_show_screen(screen_t screen);
void ui_cycle_screen(void);
void ui_toggle_splash(void);
screen_t ui_get_current_screen(void);
void ui_update_net_status(net_state_t state, const char* ssid, const char* ip, int rssi);
void ui_update_battery(int percent, bool charging);

// Pair screen: render auth URL as QR + show LAN URL + status line.
// Call once with non-null auth_url/lan_url to (re)build the QR.
// Pass nullptr to leave a field untouched.
void ui_pair_set(const char* auth_url, const char* lan_url, const char* status);
