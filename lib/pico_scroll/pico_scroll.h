#pragma once

#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
#include "pico_graphics.hpp"
#include <string>

namespace pimoroni {

class PicoScroll {
  private:
    static const uint8_t DEFAULT_ADDRESS = 0x74;
    static const uint BUFFER_SIZE = 144;

  public:
    static const int WIDTH = 17;
    static const int HEIGHT = 7;
    static const uint8_t A = 12;
    static const uint8_t B = 13;
    static const uint8_t X = 14;
    static const uint8_t Y = 15;

  private:
    uint8_t __fb[BUFFER_SIZE];

  public:
    ~PicoScroll();
    void init();
    void update();
    void set_pixels(const char *pixels);
    void set_bitmap_1d(const char *bitmap, size_t bitmap_len, int brightness,
                       int offset);
    void scroll_text(const char *text, size_t text_len, int brightness,
                     int delay_ms);
    void scroll_text(std::string text, int brightness, int delay_ms = 100) {
        scroll_text(text.c_str(), text.length(), brightness, delay_ms);
    };
    void set_text(const char *text, size_t text_len, int brightness,
                  int offset);
    void set_text(std::string text, int brightness, int offset = 0) {
        set_text(text.c_str(), text.length(), brightness, offset);
    }
    void set_pixel(uint8_t x, uint8_t y, uint8_t v);
    void clear();
    bool is_pressed(uint8_t button);

    void update(PicoGraphics *graphics);

  private:
    void i2c_write(uint8_t reg, const char *data, uint8_t len);
};

} // namespace pimoroni

extern "C" {
#endif

#define PICO_SCROLL_A ((uint8_t)12)
#define PICO_SCROLL_B ((uint8_t)13)
#define PICO_SCROLL_X ((uint8_t)14)
#define PICO_SCROLL_Y ((uint8_t)15)

int pico_scroll_init(void);
void pico_scroll_clear(void);
void pico_scroll_set_text(const char *text, uint8_t brightness);
void pico_scroll_set_pixel(uint8_t x, uint8_t y, uint8_t v);
void pico_scroll_update(void);
void pico_scroll_scroll_text(const char *text, uint8_t brightness,
                             int delay_ms);
bool pico_scroll_is_pressed(uint8_t button);

#ifdef __cplusplus
}
#endif
