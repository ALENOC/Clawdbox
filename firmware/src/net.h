#pragma once
#include <Arduino.h>

enum net_state_t {
    NET_STATE_INIT,
    NET_STATE_CONNECTING,
    NET_STATE_CONNECTED,
    NET_STATE_FAILED,
    NET_STATE_PORTAL,
};

void net_init(void);
void net_tick(void);
net_state_t net_get_state(void);
const char* net_get_ssid(void);
const char* net_get_ip(void);
int net_get_rssi(void);
