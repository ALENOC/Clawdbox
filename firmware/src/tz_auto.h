#pragma once
// Query ip-api.com to get UTC offset for the device's public IP.
// Blocking (~1-2s). Call once after WiFi connects.
void tz_auto_sync(void);
