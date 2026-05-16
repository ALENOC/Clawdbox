#pragma once
#include <stdint.h>
#include <stdbool.h>

void    bl_init(uint8_t initial_brightness);
void    bl_on(void);
void    bl_set(uint8_t val);
uint8_t bl_get(void);
void    bl_standby(void);
void    bl_wake(void);
bool    bl_is_standby(void);
