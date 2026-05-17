#pragma once

// Thin compatibility shim — all pin/board defines now live in board_cfg.h.
// Include this header only for display driver access (bus, gfx).
#include "board_cfg.h"
#include <Arduino_GFX_Library.h>
#include <Wire.h>

extern Arduino_DataBus *bus;
extern Arduino_GFX     *gfx;
