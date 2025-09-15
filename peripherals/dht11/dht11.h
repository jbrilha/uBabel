#ifndef DHT11_H
#define DHT11_H

#include <stdbool.h>
#include <stdint.h>

// typedef struct {
//     float humidity;
//     float temp_celsius;
// } dht11_reading_t;

void dht_init(int pin);

bool dht_read_data(int16_t *humidity, int16_t *temperature);
bool dht_read_float_data(float *humidity, float *temperature);

void run_dht(void);

#endif // !DHT11_H

