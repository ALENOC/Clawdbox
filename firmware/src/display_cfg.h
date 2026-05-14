#pragma once

#include <Arduino_GFX_Library.h>
#include <Wire.h>

#if defined(BOARD_S3BOX)

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

#else

#include <TouchDrvCSTXXX.hpp>
#include <XPowersLib.h>
#include <SensorQMI8658.hpp>

#define LCD_WIDTH   480
#define LCD_HEIGHT  480

#define LCD_CS      12
#define LCD_SCLK    38
#define LCD_SDIO0   4
#define LCD_SDIO1   5
#define LCD_SDIO2   6
#define LCD_SDIO3   7
#define LCD_RESET   2

#define IIC_SDA     15
#define IIC_SCL     14
#define TP_INT      11
#define TP_RST      2
#define CST9220_ADDR 0x5A

#define AXP2101_ADDR 0x34

extern Arduino_DataBus *bus;
extern Arduino_CO5300 *gfx;
extern TouchDrvCST92xx touch;
extern XPowersPMU pmu;
extern SensorQMI8658 imu;

#endif
