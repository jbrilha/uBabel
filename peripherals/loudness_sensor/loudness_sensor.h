#ifndef LOUDNESS_SENSOR_H
#define LOUDNESS_SENSOR_H

#include <stdbool.h>

bool loudness_sensor_init(int pin);

int loudness_sensor_get_reading(void);

#endif // !LOUDNESS_SENSOR_H
