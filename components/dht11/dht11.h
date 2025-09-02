#ifndef DHT11_H
#define DHT11_H

typedef struct {
    float humidity;
    float temp_celsius;
} dht11_reading_t;

void run_dht(void);

#endif // !DHT11_H

