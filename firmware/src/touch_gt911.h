#pragma once
#include <stdint.h>
#include <stdbool.h>

// Minimal GT911 driver for ESP32-S3-BOX-3 (newer panel revision).
// GT911 I2C address: 0x5D (INT=LOW at RESET release) or 0x14 (INT=HIGH).
// Register addresses are 16-bit, big-endian on the wire.

bool gt911_init(void);
bool gt911_read(uint16_t *x, uint16_t *y);
