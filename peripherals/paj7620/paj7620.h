#ifndef PAJ7620_H
#define PAJ7620_H

#include <stdbool.h>

#define PAJ7620_ADDR 0x73
#define PAJ7620_GESTURE_COUNT 9

#define PAJ7620_REG_BANK_SEL  0xEF
#define PAJ7620_REG_RESULT_L  0x43
#define PAJ7620_REG_RESULT_H  0x44

typedef enum paj7620_gesture_type {
   PAJ7620_UP,
   PAJ7620_DOWN,
   PAJ7620_LEFT,
   PAJ7620_RIGHT,
   PAJ7620_PUSH,
   PAJ7620_POLL,
   PAJ7620_CLOCKWISE,
   PAJ7620_ANTI_CLOCKWISE,
   PAJ7620_WAVE
} paj7620_gesture_t;

typedef enum paj7620_report_mode {
    FAR_240FPS,
    FAR_120FPS,
    NEAR_240FPS,
    NEAR_120FPS
} paj7620_report_t;

bool PAJ7620_init(void);

bool PAJ7620_get_gesture(paj7620_gesture_t *gesture);

#endif // !PAJ7620_H
