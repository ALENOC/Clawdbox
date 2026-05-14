#pragma once

#include <Arduino_GFX_Library.h>
#include <Wire.h>

#define LCD_WIDTH   320
#define LCD_HEIGHT  240

#define LCD_CS      5
#define LCD_DC      4
#define LCD_SCLK    7
#define LCD_MOSI    6
#define LCD_RESET   48
#define LCD_BL      45

#define IIC_SDA     8
#define IIC_SCL     18
#define TP_INT      3
#define TP_RST      48
#define TT21100_ADDR 0x24

#define BTN_BOOT    0
#define BTN_MUTE    1

extern Arduino_DataBus *bus;
extern Arduino_GFX     *gfx;
