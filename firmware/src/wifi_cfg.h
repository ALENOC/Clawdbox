#pragma once
#include <Arduino.h>

struct WifiCfg {
    String ssid;
    String psk;
    String access_token;
    String refresh_token;
    uint64_t expires_at_ms;  // unix epoch ms; 0 = unknown
};

void cfg_init(void);
bool cfg_load(WifiCfg* out);
bool cfg_save(const WifiCfg& in);
void cfg_clear(void);
bool cfg_is_provisioned(void);                  // true if ssid + access_token present
bool cfg_update_tokens(const String& access,
                       const String& refresh,
                       uint64_t expires_at_ms); // rotate after OAuth refresh
