#include "pico_scroll_wrapper.h"
#include "pico_scroll.hpp"

using namespace pimoroni;

static PicoScroll *scroll = nullptr;

extern "C" {
int pico_scroll_init(void) {
    scroll = new PicoScroll();
    scroll->init();
    return 0;
}

void pico_scroll_clear(void) {
    if (scroll) {
        scroll->clear();
    }
}

void pico_scroll_set_text(const char *text, uint8_t brightness) {
    if (scroll) {
        scroll->set_text(text, brightness);
    }
}

void pico_scroll_set_pixel(uint8_t x, uint8_t y, uint8_t v) {
    if (scroll) {
        scroll->set_pixel(x, y, v);
    }
}

void pico_scroll_update(void) {
    if (scroll) {
        scroll->update();
    }
}

void pico_scroll_scroll_text(const char *text, uint8_t brightness,
                             int delay_ms) {
    if (scroll) {
        scroll->scroll_text(text, brightness, delay_ms);
    }
}

bool pico_scroll_is_pressed(uint8_t button) {
    if (scroll) {
        return scroll->is_pressed(button);
    }

    return false;
}
}
