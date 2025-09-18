#ifndef LCD16x2_H
#define LCD16x2_H

#include <stdbool.h>
#include <stdint.h>

#define LCD_TXT_ADDR 0x3e

#define LCD_CMD_ADDR 0x80
#define LCD_DAT_ADDR 0x40

#define LCD_CLR_DISP 0x01
#define LCD_RET_HOME 0x02
#define LCD_ENTRY_MODE_SET 0x04
#define LCD_DISP_CTL 0x08
#define LCD_CURS_SHIFT 0x10
#define LCD_FUNC_SET 0x20
#define LCD_SET_CGRAM_ADDR 0x40
#define LCD_SET_DDRAM_ADDR 0x80

#define LCD_DISP_FUNC (0x20 | 0x08)
#define LCD_DISP_ON (0x04)
#define LCD_DISP_OFF (0x00)
#define LCD_CURS_ON (0x02)
#define LCD_CURS_OFF (0x00)
#define LCD_BLNK_ON (0x01)
#define LCD_BLNK_OFF (0x00)

#define LCD_ENTRY_RIGHT (0x00)
#define LCD_ENTRY_LEFT (0x02)
#define LCD_ENTRY_SHIFT_INC (0x01)
#define LCD_ENTRY_SHIFT_DEC (0x00)

bool LCD16x2_init(void);

bool LCD16x2_set_text(const char *str);
bool LCD16x2_set_scrolling_text(const char *str, const int scroll_period,
                                int padding);
bool LCD16x2_set_blinking_text(const char *str, const int blink_period);

bool LCD16x2_clear(void);

bool LCD16x2_reset_cursor(void);

bool LCD16x2_display_on(void);
bool LCD16x2_display_off(void);

bool LCD16x2_set_cursor(uint8_t col, uint8_t row);

#endif // !LCD16x2_H
