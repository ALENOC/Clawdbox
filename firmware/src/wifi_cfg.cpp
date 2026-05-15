#include "wifi_cfg.h"
#include <Preferences.h>

// Tokens are stored in NVS in plaintext. Without flash encryption + secure
// boot, NVS encryption alone is security theater (the key lives in the same
// flash). For a desk-side personal device the threat model doesn't justify
// the eFuse-burning combo. If the device is lost or repurposed, run a config
// reset (long-press BOOT >5s) to clear NVS, then revoke the token at
// console.anthropic.com.
static Preferences prefs;
static const char* NS = "clawd";

void cfg_init(void) {
    prefs.begin(NS, false);
}

bool cfg_load(WifiCfg* out) {
    if (!out) return false;
    out->ssid          = prefs.getString("ssid", "");
    out->psk           = prefs.getString("psk",  "");
    out->access_token  = prefs.getString("at",   "");
    out->refresh_token = prefs.getString("rt",   "");
    out->expires_at_ms = prefs.getULong64("exp", 0);
    return out->ssid.length() > 0;
}

bool cfg_save(const WifiCfg& in) {
    prefs.putString("ssid", in.ssid);
    prefs.putString("psk",  in.psk);
    prefs.putString("at",   in.access_token);
    prefs.putString("rt",   in.refresh_token);
    prefs.putULong64("exp", in.expires_at_ms);
    return true;
}

void cfg_clear(void) {
    prefs.clear();
}

bool cfg_is_provisioned(void) {
    return prefs.getString("ssid", "").length() > 0 &&
           prefs.getString("at",   "").length() > 0;
}

bool cfg_update_tokens(const String& access, const String& refresh, uint64_t expires_at_ms) {
    prefs.putString("at",   access);
    prefs.putString("rt",   refresh);
    prefs.putULong64("exp", expires_at_ms);
    return true;
}
