#include "lcd16x2.h"
#include "i2c_hal.h"
#include "platform.h"

static i2c_device_handle_t dev_handle;
static uint8_t disp_ctl = 0;
static uint8_t disp_mode = 0;

static bool LCD_send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {LCD_CMD_ADDR, cmd};
    return i2c_write_bytes(&dev_handle, buf, 2);
}

static bool LCD_send_data(uint8_t data) {
    uint8_t buf[2] = {LCD_DAT_ADDR, data};
    return i2c_write_bytes(&dev_handle, buf, 2);
}

bool LCD16x2_set_text(const char *str) {
    while (*str) {
        if (!LCD_send_data(*str++)) {
            return false;
        }
    }

    return true;
}

bool LCD16x2_clear(void) {
    bool ret = LCD_send_cmd(LCD_CLR_DISP);
    vTaskDelay(pdMS_TO_TICKS(2));
    return ret;
}

bool LCD16x2_reset_cursor(void) {
    bool ret = LCD_send_cmd(LCD_RET_HOME);
    vTaskDelay(pdMS_TO_TICKS(2));
    return ret;
}

bool LCD16x2_display_on(void) {
    disp_ctl |= LCD_DISP_ON;

    return LCD_send_cmd(LCD_DISP_CTL | disp_ctl);
}

bool LCD16x2_display_off(void) {
    disp_ctl &= ~LCD_DISP_ON;

    return LCD_send_cmd(LCD_DISP_CTL | disp_ctl);
}

// the timing stuff from below is based on the HD44780 datasheet
bool LCD16x2_init(void) {
    if (!i2c_create_device(&dev_handle, LCD_TXT_ADDR)) {
        return false;
    }

    vTaskDelay(pdMS_TO_TICKS(50)); // wait more than 40ms

    LCD_send_cmd(LCD_DISP_FUNC);
    vTaskDelay(pdMS_TO_TICKS(5)); // wait more than 4.1ms

    LCD_send_cmd(LCD_DISP_FUNC);
    vTaskDelay(pdMS_TO_TICKS(1)); // wait more than 100us

    LCD_send_cmd(LCD_DISP_FUNC);

    if (!LCD_send_cmd(LCD_DISP_FUNC)) {
        return false;
    }

    disp_ctl = LCD_DISP_ON | LCD_CURS_OFF | LCD_BLNK_OFF;
    if (!LCD16x2_display_on()) {
        return false;
    }

    if (!LCD16x2_clear()) {
        return false;
    }

    disp_mode = LCD_ENTRY_LEFT | LCD_ENTRY_SHIFT_DEC;
    if (!LCD_send_cmd(LCD_ENTRY_MODE_SET | disp_mode)) {
        return false;
    }

    // necessary otherwise some chars get "eaten" if set_text is called right away
    vTaskDelay(pdMS_TO_TICKS(10));

    return true;
}
