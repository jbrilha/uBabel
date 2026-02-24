#include "gpio_hal.h"

void hal_sleep_us(int us) { sleep_us(us); }

uint32_t hal_millis(void) { return to_ms_since_boot(get_absolute_time()); }
