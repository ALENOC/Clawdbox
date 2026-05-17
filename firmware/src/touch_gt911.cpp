#include "touch_gt911.h"
#include "board_cfg.h"
#include <Wire.h>
#include <Arduino.h>

// GT911 register map
#define GT911_REG_PID     0x8140   // 4-byte ASCII product ID ("911\0")
#define GT911_REG_STATUS  0x814E   // touch buffer status (bit7=ready, [3:0]=count)
#define GT911_REG_PT1     0x8150   // first touch point (8 bytes)

// Each touch point record (8 bytes):
//   [0]    track ID
//   [1-2]  X  (little-endian)
//   [3-4]  Y  (little-endian)
//   [5-6]  size (little-endian)
//   [7]    reserved

static uint8_t s_addr = GT911_ADDR_A;

static bool reg_write(uint16_t reg, uint8_t val) {
    Wire.beginTransmission(s_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static bool reg_read(uint16_t reg, uint8_t *buf, uint8_t len) {
    Wire.beginTransmission(s_addr);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;  // repeated start
    Wire.requestFrom(s_addr, (uint8_t)len);
    uint8_t n = 0;
    while (Wire.available() && n < len) buf[n++] = Wire.read();
    return n == len;
}

bool gt911_init(void) {
    // Determine active address
    Wire.beginTransmission(GT911_ADDR_A);
    if (Wire.endTransmission() == 0) {
        s_addr = GT911_ADDR_A;
    } else {
        Wire.beginTransmission(GT911_ADDR_B);
        if (Wire.endTransmission() == 0) {
            s_addr = GT911_ADDR_B;
        } else {
            Serial.println("GT911: not found on any address");
            return false;
        }
    }

    uint8_t pid[4] = {};
    if (reg_read(GT911_REG_PID, pid, 4)) {
        Serial.printf("GT911: PID=%c%c%c addr=0x%02X\n",
                      pid[0], pid[1], pid[2], s_addr);
    }
    return true;
}

bool gt911_read(uint16_t *x, uint16_t *y) {
    uint8_t status;
    if (!reg_read(GT911_REG_STATUS, &status, 1)) return false;

    uint8_t count = status & 0x0F;
    if (!(status & 0x80) || count == 0) return false;

    uint8_t pt[8];
    bool ok = reg_read(GT911_REG_PT1, pt, 8);
    reg_write(GT911_REG_STATUS, 0x00);  // always clear buffer
    if (!ok) return false;

    uint16_t px = (uint16_t)pt[1] | ((uint16_t)pt[2] << 8);
    uint16_t py = (uint16_t)pt[3] | ((uint16_t)pt[4] << 8);
    if (px >= LCD_WIDTH || py >= LCD_HEIGHT) return false;

    *x = px;
    *y = py;
    return true;
}
