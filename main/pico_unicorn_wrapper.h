#ifndef PICO_UNICORN_WRAPPER_H
#define PICO_UNICORN_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICO_SCROLL_A ((uint8_t)12)
#define PICO_SCROLL_B ((uint8_t)13)
#define PICO_SCROLL_X ((uint8_t)14)
#define PICO_SCROLL_Y ((uint8_t)15)

int pico_unicorn_init(void);
void pico_unicorn_clear(void);
void pico_unicorn_set_pixel(uint8_t x, uint8_t y, uint8_t v);
void pico_unicorn_set_pixel_rgb(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
bool pico_unicorn_is_pressed(uint8_t button);

#ifdef __cplusplus
}
#endif

#endif
