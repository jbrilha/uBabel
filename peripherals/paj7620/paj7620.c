#include "paj7620.h"
#include "i2c_hal.h"
#include "platform.h"
#include <stdint.h>

static const char *TAG = "PAJ7620";

// clang-format off
 static const uint8_t init_register_arr[][2] = {
        // BANK 0
        {0xEF,0x00}, {0x37,0x07}, {0x38,0x17}, {0x39,0x06}, {0x42,0x01}, 
        {0x46,0x2D}, {0x47,0x0F}, {0x48,0x3C}, {0x49,0x00}, {0x4A,0x1E}, 
        {0x4C,0x20}, {0x51,0x10}, {0x5E,0x10}, {0x60,0x27}, {0x80,0x42}, 
        {0x81,0x44}, {0x82,0x04}, {0x8B,0x01}, {0x90,0x06}, {0x95,0x0A}, 
        {0x96,0x0C}, {0x97,0x05}, {0x9A,0x14}, {0x9C,0x3F}, {0xA5,0x19}, 
        {0xCC,0x19}, {0xCD,0x0B}, {0xCE,0x13}, {0xCF,0x64}, {0xD0,0x21}, 
        // BANK 1
        {0xEF,0x01}, {0x02,0x0F}, {0x03,0x10}, {0x04,0x02}, {0x25,0x01},
        {0x27,0x39}, {0x28,0x7F}, {0x29,0x08}, {0x3E,0xFF}, {0x5E,0x3D}, 
        {0x65,0x96}, {0x67,0x97}, {0x69,0xCD}, {0x6A,0x01}, {0x6D,0x2C}, 
        {0x6E,0x01}, {0x72,0x01}, {0x73,0x35}, {0x77,0x01}, {0xEF,0x00},
    };

#define INIT_REG_ARRAY_SIZE (sizeof(init_register_arr)/sizeof(init_register_arr[0]))
// clang-format on

static i2c_device_handle_t device;

static bool read_reg(uint8_t reg_addr, uint8_t *val) {
    return i2c_read_register(&device, reg_addr, val, 1);
}

static bool write_reg(uint8_t reg_addr, uint8_t val) {
    return i2c_write_register(&device, reg_addr, &val, 1);
}

static bool set_report_mode(uint8_t mode) {
    uint8_t reg_idle_time = 0;
    write_reg(PAJ7620_REG_BANK_SEL, 1); // reg in Bank1
    switch (mode) {
        // Far Mode: 1 report time = (77 + R_IDLE_TIME) * T
    case FAR_240FPS:
        reg_idle_time = 53; // 1/(240*T) - 77
        break;
    case FAR_120FPS:
        reg_idle_time = 183; // 1/(120*T) - 77
        break;
        // Near Mode: 1 report time = (112 + R_IDLE_TIME) * T
    case NEAR_240FPS:
        reg_idle_time = 18; // 1/(240*T) - 112
        break;
    case NEAR_120FPS:
        reg_idle_time = 148; // 1/(120*T) - 112
        break;
    default:
        return false;
    }
    write_reg(0x65, reg_idle_time);
    write_reg(PAJ7620_REG_BANK_SEL, 0); // reg in Bank0
    return true;
}

bool PAJ7620_init(void) {
    if (!i2c_create_device(&device, PAJ7620_ADDR)) {
        printf("a\n");
        return false;
    }

    write_reg(0xFF, 0x00);

    vTaskDelay(pdMS_TO_TICKS(50));

    uint8_t reg_00, reg_01;
    read_reg(0x01, &reg_01);
    read_reg(0x00, &reg_00);

    if ((reg_01 != 0x76) || (reg_00 != 0x20)) {
        printf("b\n");
        return false;
    }

    for (uint8_t i = 0; i < INIT_REG_ARRAY_SIZE; i++)
        write_reg(init_register_arr[i][0], init_register_arr[i][1]);

    return set_report_mode(FAR_240FPS);
}

bool PAJ7620_get_gesture(paj7620_gesture_t *gesture) {
    uint8_t reg_h, reg_l;
    read_reg(PAJ7620_REG_RESULT_H, &reg_h);
    read_reg(PAJ7620_REG_RESULT_L, &reg_l);

    uint16_t code = (reg_h << 8) + reg_l;

    if (code == 0)
        return false;

    for (uint8_t i = PAJ7620_UP; i < PAJ7620_GESTURE_COUNT; i++) {
        if (code == (1 << i)) {
            *gesture = (paj7620_gesture_t)i;
            read_reg(PAJ7620_REG_RESULT_H, &reg_h);
            read_reg(PAJ7620_REG_RESULT_L, &reg_l);
            return true;
        }
    }

    return false;
}

void PAJ7620_task(void *params) {
    if (PAJ7620_init()) {
        while (true) {
            paj7620_gesture_t gesture;
            if(PAJ7620_get_gesture(&gesture)) {
                switch (gesture) {
                case PAJ7620_UP:
                    printf("—————— UP ——————\n");
                    break;
                case PAJ7620_DOWN:
                    printf("————— DOWN —————\n");
                    break;
                case PAJ7620_LEFT:
                    printf("————— LEFT —————\n");
                    break;
                case PAJ7620_RIGHT:
                    printf("————— RIGHT —————\n");
                    break;
                case PAJ7620_PUSH:
                    printf("—————— PUSH ——————\n");
                    break;
                case PAJ7620_PULL:
                    printf("————— PULL —————\n");
                    break;
                case PAJ7620_CLOCKWISE:
                    printf("————— CLOCK —————\n");
                    break;
                case PAJ7620_ANTI_CLOCKWISE:
                    printf("————— ANTI —————\n");
                    break;
                case PAJ7620_WAVE:
                    printf("————— WAVE —————\n");
                    break;
                    break;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(500));
        }

    } else {
        LOG_ERROR(TAG, "Failed to init PAJ7620 gesture detector");
    }

    vTaskDelete(NULL);
}

void run_PAJ7620_task(void) {

    xTaskCreate(PAJ7620_task,   // Task function
                "PAJ7620_task", // Task name
                4096,           // Stack size in words
                NULL,           // Task parameters
                5,              // Priority
                NULL            // Task handle
    );
}
