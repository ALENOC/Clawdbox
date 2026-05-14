#include "imu.h"

// ESP32-S3-BOX has no IMU. Auto-rotation disabled; display stays in
// its single landscape orientation.
void    imu_init(void)         {}
void    imu_tick(void)         {}
uint8_t imu_get_rotation(void) { return 0; }
