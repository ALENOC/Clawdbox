#include "touch_tt21100.h"
#include "display_cfg.h"
#include <Arduino.h>
#include <Wire.h>


// TT21100 touch frame (empirical layout on the BOX panel):
//   [0..1]  data_len (LE) — 17 for one touch present, ~7 for idle
//   [2]     report_id (0x01 = touch)
//   [3..4]  timestamp
//   [5..6]  flags / counter
//   [7..8]  touch state / event id
//   [9..10] x  (LE)
//   [11..12] y (LE)
//   [13]    pressure
// We do a single I2C read of 32 bytes and parse from there. Reads with
// data_len != 17 are treated as "no touch".

bool tt21100_init(void) {
    Wire.beginTransmission(TT21100_ADDR);
    if (Wire.endTransmission() != 0) {
        Serial.println("TT21100 not responding");
        return false;
    }
    Serial.println("TT21100 init OK");
    return true;
}

bool tt21100_read(uint16_t *x, uint16_t *y) {
    static const uint8_t READ_LEN = 32;
    uint8_t buf[READ_LEN];
    Wire.requestFrom((int)TT21100_ADDR, (int)READ_LEN);
    uint8_t n = 0;
    while (Wire.available() && n < READ_LEN) buf[n++] = Wire.read();
    if (n < 13) return false;

    uint16_t data_len = (uint16_t)buf[0] | ((uint16_t)buf[1] << 8);
    if (data_len != 17) return false;
    if (buf[2] != 0x01) return false;

    uint16_t cx = (uint16_t)buf[9]  | ((uint16_t)buf[10] << 8);
    uint16_t cy = (uint16_t)buf[11] | ((uint16_t)buf[12] << 8);
    if (cx >= 320 || cy >= 240) return false;

    *x = cx;
    *y = cy;
    return true;
}

