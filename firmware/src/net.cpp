#include "net.h"
#include "wifi_cfg.h"
#include "portal.h"
#include <WiFi.h>
#include <time.h>

static net_state_t state = NET_STATE_INIT;
static String cur_ssid = "";
static String cur_ip   = "";
static uint32_t connect_started_ms = 0;
static int attempts = 0;
static const int MAX_ATTEMPTS = 3;
static const uint32_t TIMEOUT_MS = 15000;

static void start_portal_mode(void) {
    state = NET_STATE_PORTAL;
    portal_start();
    cur_ssid = portal_ssid();
    cur_ip   = "192.168.4.1";
}

static void begin_sta(const WifiCfg& c) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(c.ssid.c_str(), c.psk.c_str());
    cur_ssid = c.ssid;
    cur_ip   = "";
    state = NET_STATE_CONNECTING;
    connect_started_ms = millis();
}

void net_init(void) {
    // Portal is for WiFi only. OAuth pairing happens later, in STA mode,
    // via the on-device QR flow (pair.cpp).
    if (!cfg_has_wifi()) {
        Serial.println("net: no wifi config, starting portal");
        start_portal_mode();
        return;
    }
    WifiCfg c;
    cfg_load(&c);
    Serial.printf("net: connecting to %s\n", c.ssid.c_str());
    begin_sta(c);
}

void net_tick(void) {
    if (state == NET_STATE_PORTAL) {
        portal_tick();
        return;
    }
    if (state == NET_STATE_CONNECTING) {
        if (WiFi.status() == WL_CONNECTED) {
            cur_ip = WiFi.localIP().toString();
            Serial.printf("net: connected ip=%s rssi=%d\n", cur_ip.c_str(), WiFi.RSSI());
            state = NET_STATE_CONNECTED;
            attempts = 0;
            // Start SNTP only now — calling configTime() before the link is
            // up triggers a LWIP TCPIP-lock assertion on first sync.
            static bool sntp_started = false;
            if (!sntp_started) {
                configTime(0, 0, "pool.ntp.org", "time.google.com");
                sntp_started = true;
            }
            return;
        }
        if (millis() - connect_started_ms > TIMEOUT_MS) {
            attempts++;
            Serial.printf("net: connect attempt %d failed\n", attempts);
            WiFi.disconnect(true);
            if (attempts >= MAX_ATTEMPTS) {
                Serial.println("net: giving up, falling back to portal");
                start_portal_mode();
                return;
            }
            WifiCfg c;
            cfg_load(&c);
            begin_sta(c);
        }
        return;
    }
    if (state == NET_STATE_CONNECTED) {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("net: link lost, reconnecting");
            WifiCfg c;
            cfg_load(&c);
            begin_sta(c);
        }
    }
}

net_state_t net_get_state(void) { return state; }
const char* net_get_ssid(void)  { return cur_ssid.c_str(); }
const char* net_get_ip(void)    { return cur_ip.c_str(); }
int net_get_rssi(void) {
    return state == NET_STATE_CONNECTED ? WiFi.RSSI() : 0;
}
