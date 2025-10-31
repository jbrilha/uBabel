#ifndef M5_BUTTONS_H
#define M5_BUTTONS_H

#define M5_PIN_A 39
#define M5_PIN_B 38
#define M5_PIN_C 37

// to avoid conflicts with the pico events
#define EVENT_M5_BUTTON_C_PRESSED (M5_PIN_C + 200) // 237
#define EVENT_M5_BUTTON_B_PRESSED (M5_PIN_B + 200) // 238
#define EVENT_M5_BUTTON_A_PRESSED (M5_PIN_A + 200) // 239

void m5_buttons_init(void);
void run_m5_buttons_task(void);

#endif // !M5_BUTTONS_H

