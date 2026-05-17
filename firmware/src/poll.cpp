#include "poll.h"
#include "wifi_cfg.h"
#include "oauth.h"
#include "net.h"
#include "anthropic_ca.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <time.h>

static const char* API_URL = "https://api.anthropic.com/v1/messages";
static const uint32_t POLL_PERIOD_MS = 60000;
static uint32_t last_poll_ms = 0;
static bool first_poll_done = false;

// Tiny request body — 1 max token, no actual completion needed.
// We only care about the rate-limit response headers.
static const char* PROBE_BODY =
    "{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1,"
    "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}";

static int ceil_pct(float frac) {
    int x = (int)(frac * 100.0f);
    if ((float)x < frac * 100.0f) x++;
    return x;
}

static int reset_minutes_from_epoch(const String& s) {
    if (s.length() == 0) return -1;
    uint32_t reset = (uint32_t)strtoul(s.c_str(), nullptr, 10);
    time_t now = time(nullptr);
    if (now < 1704067200) return -1;  // NTP not synced yet (pre-2024)
    if (reset <= (uint32_t)now) return 0;
    return (int)((reset - now) / 60);
}

static bool do_poll(UsageData* out) {
    if (net_get_state() != NET_STATE_CONNECTED) return false;
    if (!oauth_ensure_valid(60000)) return false;

    WifiCfg c;
    if (!cfg_load(&c)) return false;

    WiFiClientSecure client;
    client.setCACert(ANTHROPIC_ROOT_CA);

    HTTPClient http;
    if (!http.begin(client, API_URL)) {
        Serial.println("poll: http.begin failed");
        return false;
    }
    http.addHeader("Authorization", "Bearer " + c.access_token);
    http.addHeader("anthropic-version", "2023-06-01");
    http.addHeader("anthropic-beta", "oauth-2025-04-20");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("User-Agent", "claude-code/2.1.5");

    const char* hdr_names[] = {
        "anthropic-ratelimit-unified-5h-utilization",
        "anthropic-ratelimit-unified-5h-reset",
        "anthropic-ratelimit-unified-7d-utilization",
        "anthropic-ratelimit-unified-7d-reset",
        "anthropic-ratelimit-unified-5h-status",
    };
    http.collectHeaders(hdr_names, 5);

    int code = http.POST((uint8_t*)PROBE_BODY, strlen(PROBE_BODY));
    if (code <= 0) {
        Serial.printf("poll: HTTP error %d\n", code);
        http.end();
        return false;
    }

    String s5h_util  = http.header("anthropic-ratelimit-unified-5h-utilization");
    String s5h_reset = http.header("anthropic-ratelimit-unified-5h-reset");
    String s7d_util  = http.header("anthropic-ratelimit-unified-7d-utilization");
    String s7d_reset = http.header("anthropic-ratelimit-unified-7d-reset");
    String status    = http.header("anthropic-ratelimit-unified-5h-status");
    http.end();

    if (code != 200 && code != 201) {
        Serial.printf("poll: HTTP %d (headers may still be valid)\n", code);
        // Anthropic returns rate-limit headers on most responses, including
        // 4xx — still try to parse if util headers present.
        if (s5h_util.length() == 0) return false;
    }

    out->session_pct       = ceil_pct(s5h_util.toFloat());
    out->session_reset_mins = reset_minutes_from_epoch(s5h_reset);
    out->weekly_pct        = ceil_pct(s7d_util.toFloat());
    out->weekly_reset_mins = reset_minutes_from_epoch(s7d_reset);
    strlcpy(out->status, status.length() ? status.c_str() : "unknown", sizeof(out->status));
    out->ok = true;
    out->valid = true;

    Serial.printf("poll: s=%.0f%% sr=%d w=%.0f%% wr=%d st=%s\n",
                  out->session_pct, out->session_reset_mins,
                  out->weekly_pct,  out->weekly_reset_mins,
                  out->status);
    return true;
}

void poll_init(void) {
    // NTP is started in net.cpp after WiFi associates. Calling configTime()
    // before WiFi is up triggers SNTP/UDP from outside the TCPIP core lock
    // and crashes the LWIP assertion.
}

void poll_tick(UsageData* out, bool* updated) {
    *updated = false;
    if (net_get_state() != NET_STATE_CONNECTED) return;
    uint32_t now = millis();
    bool due = !first_poll_done || (now - last_poll_ms >= POLL_PERIOD_MS);
    if (!due) return;

    if (do_poll(out)) {
        last_poll_ms = now;
        first_poll_done = true;
        *updated = true;
    } else {
        // Backoff: try again in 10s instead of waiting full period.
        last_poll_ms = now - (POLL_PERIOD_MS - 10000);
    }
}

bool poll_force_now(UsageData* out) {
    if (do_poll(out)) {
        last_poll_ms = millis();
        first_poll_done = true;
        return true;
    }
    return false;
}

void poll_reset_timer(void) {
    first_poll_done = false;
}
