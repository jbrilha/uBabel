#ifndef TIME_HAL_H
#define TIME_HAL_H

#include <stdint.h>

void hal_sleep_us(int us);

uint32_t hal_millis();

#endif // !TIME_HAL_H
