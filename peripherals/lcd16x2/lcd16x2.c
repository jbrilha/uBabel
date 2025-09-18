#include "lcd16x2.h"

#include "i2c_hal.h"
#include "platform.h"
#include <stdlib.h>
#include <string.h>

#define MAX_LCD_STR_LEN 128
#define LCD_CHAR_LIMIT 32
#define PADDING_LIMIT 16

typedef struct {
    char *str;
    int scroll_period;
} scroll_ctx_t;

typedef struct {
    char *str;
    int blink_period;
} blink_ctx_t;

static i2c_device_handle_t dev_handle;
static uint8_t disp_ctl = 0;
static uint8_t disp_mode = 0;

static TaskHandle_t scroll_task_handle = NULL;
static TaskHandle_t blink_task_handle = NULL;

static void clear_tasks(void) {
    if (blink_task_handle != NULL) {
        vTaskDelete(blink_task_handle);
        blink_task_handle = NULL;
    }

    if (scroll_task_handle != NULL) {
        vTaskDelete(scroll_task_handle);
        scroll_task_handle = NULL;
    }
}

static bool LCD_send_cmd(uint8_t cmd) {
    uint8_t buf[2] = {LCD_CMD_ADDR, cmd};
    return i2c_write_bytes(&dev_handle, buf, 2);
}

static bool LCD_send_data(uint8_t data) {
    uint8_t buf[2] = {LCD_DAT_ADDR, data};
    return i2c_write_bytes(&dev_handle, buf, 2);
}

static bool set_text(const char *str) {
    uint8_t idx = 0, row = 0;

    while (*str) {
        if (*str == '\n') {
            idx = 0;
            row = 1;
            if (!LCD16x2_set_cursor(0, row)) {
                return false;
            }
        } else {
            if (idx >= 16) {
                if (row >= 1) {
                    break;
                }

                idx = 0;
                row = (row + 1) % 2;
                if (!LCD16x2_set_cursor(0, row)) {
                    return false;
                }
            }
            if (!LCD_send_data(*str)) {
                return false;
            }
            idx++;
        }
        str++;
    }

    if (!LCD16x2_reset_cursor()) {
        return false;
    }

    return true;
}

bool LCD16x2_set_text(const char *str) {
    clear_tasks();

    return set_text(str);
}

void scroll_text_task(void *params) {
    scroll_ctx_t *ctx = (scroll_ctx_t *)params;
    size_t str_len = strlen(ctx->str);
    size_t i = 0;

    while (true) {
        if (!LCD16x2_clear()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(10));

        char lcd_txt[LCD_CHAR_LIMIT + 1];
        size_t chars_copied = 0;

        size_t first_part_len =
            (str_len - i > LCD_CHAR_LIMIT) ? LCD_CHAR_LIMIT : str_len - i;
        strncpy(lcd_txt, ctx->str + i, first_part_len);
        chars_copied = first_part_len;

        if (chars_copied < LCD_CHAR_LIMIT) {
            size_t remaining = LCD_CHAR_LIMIT - chars_copied;
            strncpy(lcd_txt + chars_copied, ctx->str, remaining);
            chars_copied += remaining;
        }

        lcd_txt[chars_copied] = '\0';

        if (!set_text(lcd_txt)) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(ctx->scroll_period));

        i = (i + 1) % str_len;
    }

    free(ctx->str);
    free(ctx);
    vTaskDelete(NULL);
}

bool LCD16x2_set_scrolling_text(const char *str, const int scroll_period,
                                int padding) {
    size_t input_len = strlen(str);
    if (padding > PADDING_LIMIT) {
        padding = PADDING_LIMIT;
    }
    size_t alloc_len =
        ((input_len < MAX_LCD_STR_LEN) ? input_len : MAX_LCD_STR_LEN) +
        padding + 1;

    scroll_ctx_t *ctx = (scroll_ctx_t *)malloc(sizeof(scroll_ctx_t));
    if (ctx == NULL) {
        return false;
    }

    ctx->str = (char *)malloc(alloc_len * sizeof(char));
    if (ctx->str == NULL) {
        free(ctx);
        return false;
    }

    strncpy(ctx->str, str, alloc_len - 1);
    for (size_t i = alloc_len - padding - 1; i < alloc_len - 1; i++) {
        ctx->str[i] = ' ';
    }
    ctx->str[alloc_len - 1] = '\0';
    ctx->scroll_period = scroll_period;

    clear_tasks();
    if (xTaskCreate(scroll_text_task, "scroll_text_task", 4096, (void *)ctx, 1,
                    &scroll_task_handle) == pdPASS) {
        return true;
    } else {
        free(ctx->str);
        free(ctx);
        return false;
    }
}

void blink_text_task(void *params) {
    blink_ctx_t *ctx = (blink_ctx_t *)params;

    while (true) {
        if (!LCD16x2_clear()) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(ctx->blink_period));

        if (!set_text(ctx->str)) {
            break;
        }

        vTaskDelay(pdMS_TO_TICKS(ctx->blink_period));
    }

    free(ctx->str);
    free(ctx);
    vTaskDelete(NULL);
}

bool LCD16x2_set_blinking_text(const char *str, const int blink_period) {
    size_t input_len = strlen(str);
    size_t alloc_len =
        ((input_len < LCD_CHAR_LIMIT) ? input_len : LCD_CHAR_LIMIT) + 1;

    blink_ctx_t *ctx = (blink_ctx_t *)malloc(sizeof(blink_ctx_t));
    if (ctx == NULL) {
        return false;
    }

    ctx->str = (char *)malloc(alloc_len * sizeof(char));
    if (ctx->str == NULL) {
        free(ctx);
        return false;
    }

    strncpy(ctx->str, str, alloc_len - 1);
    ctx->str[alloc_len - 1] = '\0';
    ctx->blink_period = blink_period;

    clear_tasks();
    if (xTaskCreate(blink_text_task, "blink_text_task", 4096, (void *)ctx, 1,
                    &blink_task_handle) == pdPASS) {
        return true;
    } else {
        free(ctx->str);
        free(ctx);
        return false;
    }
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

bool LCD16x2_set_cursor(uint8_t col, uint8_t row) {
    col = (row == 0 ? col | 0x80 : col | 0xc0);

    return LCD_send_cmd(col);
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

    // necessary otherwise some chars get "eaten" if set_text is called right
    // away
    vTaskDelay(pdMS_TO_TICKS(10));

    return true;
}
