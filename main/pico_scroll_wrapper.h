#ifndef PICO_SCROLL_WRAPPER_H
#define PICO_SCROLL_WRAPPER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PICO_UNICORN_A ((uint8_t)12)
#define PICO_UNICORN_B ((uint8_t)13)
#define PICO_UNICORN_X ((uint8_t)14)
#define PICO_UNICORN_Y ((uint8_t)15)

int pico_scroll_init(void);
void pico_scroll_clear(void);
void pico_scroll_set_text(const char* text, uint8_t brightness);
void pico_scroll_set_pixel(uint8_t x, uint8_t y, uint8_t v);
void pico_scroll_update(void);
void pico_scroll_scroll_text(const char* text, uint8_t brightness, int delay_ms);
bool pico_scroll_is_pressed(uint8_t button);

#ifdef __cplusplus
}
#endif

#endif
