#include "oauth.h"
#include "wifi_cfg.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

// Claude Code OAuth constants. If refresh starts returning 400/401,
// verify these against the current Claude Code CLI source.
static const char* TOKEN_URL = "https://console.anthropic.com/v1/oauth/token";
static const char* CLIENT_ID = "9d1c250a-e61b-44d9-88ed-5944d1962f5e";

static uint64_t now_ms(void) {
    time_t now = time(nullptr);
    if (now < 1700000000) return 0;  // clock not synced
    return (uint64_t)now * 1000ULL;
}

bool oauth_refresh(void) {
    WifiCfg c;
    if (!cfg_load(&c) || c.refresh_token.length() == 0) {
        Serial.println("oauth: no refresh token");
        return false;
    }

    WiFiClientSecure client;
    // FIXME: pin Anthropic CA. Acceptable on home WiFi; revisit before shipping.
    client.setInsecure();

    HTTPClient http;
    if (!http.begin(client, TOKEN_URL)) {
        Serial.println("oauth: http.begin failed");
        return false;
    }
    http.addHeader("Content-Type", "application/json");

    JsonDocument req;
    req["grant_type"]    = "refresh_token";
    req["refresh_token"] = c.refresh_token;
    req["client_id"]     = CLIENT_ID;
    String body;
    serializeJson(req, body);

    int code = http.POST(body);
    if (code != 200) {
        Serial.printf("oauth: refresh HTTP %d\n", code);
        String resp = (code > 0) ? http.getString() : String("");
        if (resp.length()) Serial.printf("oauth: body: %s\n", resp.c_str());
        http.end();
        // Permanent failures (invalid/expired refresh token) → wipe tokens so
        // the main loop transitions to pairing instead of retrying forever.
        if (code == 400 || code == 401) {
            if (resp.indexOf("invalid_grant") >= 0 ||
                resp.indexOf("invalid_request") >= 0 ||
                resp.indexOf("unauthorized") >= 0) {
                Serial.println("oauth: refresh token invalid, clearing tokens");
                cfg_clear_tokens();
            }
        }
        return false;
    }

    String resp = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, resp);
    if (err) {
        Serial.printf("oauth: parse err %s\n", err.c_str());
        return false;
    }

    String access  = doc["access_token"]  | "";
    String refresh = doc["refresh_token"] | c.refresh_token;  // some servers reuse
    uint32_t expires_in = doc["expires_in"] | 3600;

    if (access.length() == 0) {
        Serial.println("oauth: no access_token in response");
        return false;
    }

    uint64_t exp = now_ms();
    if (exp != 0) exp += (uint64_t)expires_in * 1000ULL;

    cfg_update_tokens(access, refresh, exp);
    Serial.printf("oauth: refreshed, expires_in=%u\n", expires_in);
    return true;
}

bool oauth_ensure_valid(uint32_t min_remaining_ms) {
    WifiCfg c;
    if (!cfg_load(&c) || c.access_token.length() == 0) return false;

    uint64_t now = now_ms();
    if (now == 0) {
        // Clock not yet synced — try once, the server will tell us if stale.
        return c.access_token.length() > 0;
    }
    if (c.expires_at_ms == 0 || c.expires_at_ms <= now + min_remaining_ms) {
        return oauth_refresh();
    }
    return true;
}
