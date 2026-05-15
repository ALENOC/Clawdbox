#pragma once
#include <Arduino.h>

// SoftAP captive portal for first-boot provisioning.
// Brings up AP "Clawdmeter-setup" (open) and serves a form at 192.168.4.1
// that collects SSID/PSK/OAuth tokens, persists via cfg_save(), then reboots.
void portal_start(void);
void portal_tick(void);
bool portal_is_active(void);
const char* portal_ssid(void);
