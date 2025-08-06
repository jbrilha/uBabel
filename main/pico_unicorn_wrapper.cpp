#include "pico_unicorn_wrapper.h"
#include "pico_unicorn.hpp"

using namespace pimoroni;

static PicoUnicorn *unicorn = nullptr;

extern "C" {
int pico_unicorn_init(void) {
    unicorn = new PicoUnicorn();
    unicorn->init();
    return 0;
}

void pico_unicorn_clear(void) {
    if (unicorn) {
        unicorn->clear();
    }
}

void pico_unicorn_set_pixel(uint8_t x, uint8_t y, uint8_t v) {
    if (unicorn) {
        unicorn->set_pixel(x, y, v);
    }
}

void pico_unicorn_set_pixel_rgb(uint8_t x, uint8_t y, uint8_t r, uint8_t g,
                                uint8_t b) {
    if (unicorn) {
        unicorn->set_pixel(x, y, r, g, b);
    }
}

bool pico_unicorn_is_pressed(uint8_t button) {
    if (unicorn) {
        return unicorn->is_pressed(button);
    }

    return false;
}
}
