#ifndef BMP280_H
#define BMP280_H

#include <stdbool.h>
#include <stdint.h>

#define BMP280_ADDR 0x77

bool BMP280_init(void);

#define STANDARD_SEA_LEVEL_PRESSURE (101325)

bool BMP280_get_temperature(float *temp);
bool BMP280_get_pressure(uint32_t *pressure);
bool BMP280_get_altitude(const float ref_pressure, float *altitude);

void run_BMP280_task(void);

#endif // !BMP280_H
