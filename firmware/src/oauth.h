#pragma once
#include <Arduino.h>

// Refresh OAuth access token using the saved refresh_token.
// On success: persists rotated tokens via cfg_update_tokens() and returns true.
// On failure: returns false. Caller should backoff + retry.
bool oauth_refresh(void);

// Ensure access token is valid for at least `min_remaining_ms` more.
// Calls oauth_refresh() if expiry is near. Returns true if token is usable.
bool oauth_ensure_valid(uint32_t min_remaining_ms);
