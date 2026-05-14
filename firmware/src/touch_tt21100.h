#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool tt21100_init(void);
// Reads one frame from the controller. Returns true if at least one touch
// is present and writes its coordinates to *x, *y.
bool tt21100_read(uint16_t *x, uint16_t *y);

#ifdef __cplusplus
}
#endif
