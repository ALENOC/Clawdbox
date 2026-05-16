#include "tz_auto.h"
#include "settings_mgr.h"
#include <WiFi.h>
#include <Arduino.h>

void tz_auto_sync(void) {
    WiFiClient client;
    if (!client.connect("ip-api.com", 80)) {
        Serial.println("tz_auto: connect failed");
        return;
    }
    client.print("GET /json/?fields=status,offset HTTP/1.0\r\nHost: ip-api.com\r\nConnection: close\r\n\r\n");

    uint32_t t0 = millis();
    // Skip HTTP headers
    while (client.connected() || client.available()) {
        if (millis() - t0 > 6000) { client.stop(); return; }
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.isEmpty()) break;
    }

    String body;
    t0 = millis();
    while ((client.connected() || client.available()) && millis() - t0 < 4000) {
        if (client.available()) body += (char)client.read();
    }
    client.stop();

    // Expect: {"status":"success","offset":3600}
    if (body.indexOf("\"success\"") < 0) {
        Serial.printf("tz_auto: bad response: %s\n", body.c_str());
        return;
    }
    int idx = body.indexOf("\"offset\":");
    if (idx < 0) return;
    idx += 9;
    // find end of number (comma or closing brace)
    int end = idx;
    while (end < (int)body.length() && body[end] != ',' && body[end] != '}') end++;
    int32_t offset_sec = body.substring(idx, end).toInt();
    int8_t tz = (int8_t)(offset_sec / 3600);
    if (tz < -12) tz = -12;
    if (tz >  14) tz =  14;
    settings_set_tz_offset(tz);
    Serial.printf("tz_auto: %ds → UTC%+d\n", (int)offset_sec, (int)tz);
}
