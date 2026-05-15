#pragma once
#include <Arduino.h>

enum pair_status_t {
    PAIR_IDLE,
    PAIR_WAITING,
    PAIR_EXCHANGING,
    PAIR_OK,
    PAIR_FAIL,
};

// Generate PKCE state, build authorize URL, start LAN /pair server.
// Idempotent — calling again regenerates a fresh verifier/state.
void pair_start(void);

// Drive any queued token exchange from the main loop (synchronous HTTPS).
// On success, persists tokens and reboots.
void pair_tick(void);

bool pair_is_active(void);
pair_status_t pair_get_status(void);
const char* pair_get_status_msg(void);
const char* pair_get_auth_url(void);
const char* pair_get_lan_url(void);
