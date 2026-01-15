#ifndef PCF8574_H
#define PCF8574_H

#include <stdbool.h>
#include <stdint.h>

#define PCF8574_ADDR 0x20

bool PCF8574_init(void);

uint8_t PCF8574_read_8() ;
bool PCF8574_read(const uint8_t pin) ;

bool PCF8574_write_8(uint8_t value) ;
bool PCF8574_write(const uint8_t pin, const uint8_t value);

void run_pcf_task(void);

#endif // !PCF8574_H
