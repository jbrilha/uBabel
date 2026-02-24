#include "time_hal.h"
#include <esp_timer.h>
#include <rom/ets_sys.h>

void hal_sleep_us(int us) {
    ets_delay_us(us);
}

uint32_t hal_millis(void) {
    return esp_timer_get_time() / 1000;
}
