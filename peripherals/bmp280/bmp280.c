#include "bmp280.h"
#include "i2c_hal.h"
#include "platform.h"
#include <math.h>

#define BMP280_REG_DIG_T1 0x88
#define BMP280_REG_DIG_T2 0x8A
#define BMP280_REG_DIG_T3 0x8C

#define BMP280_REG_DIG_P1 0x8E
#define BMP280_REG_DIG_P2 0x90
#define BMP280_REG_DIG_P3 0x92
#define BMP280_REG_DIG_P4 0x94
#define BMP280_REG_DIG_P5 0x96
#define BMP280_REG_DIG_P6 0x98
#define BMP280_REG_DIG_P7 0x9A
#define BMP280_REG_DIG_P8 0x9C
#define BMP280_REG_DIG_P9 0x9E

#define BMP280_REG_CHIPID 0xD0
#define BMP280_REG_VERSION 0xD1
#define BMP280_REG_SOFTRESET 0xE0

#define BMP280_REG_CONTROL 0xF4
#define BMP280_REG_CONFIG 0xF5
#define BMP280_REG_PRESSUREDATA 0xF7
#define BMP280_REG_TEMPDATA 0xFA

static const char *TAG = "BMP280";

static i2c_device_handle_t dev_handle;
static bool transport_ok;

static uint16_t dig_T1;
static int16_t dig_T2;
static int16_t dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2;
static int16_t dig_P3;
static int16_t dig_P4;
static int16_t dig_P5;
static int16_t dig_P6;
static int16_t dig_P7;
static int16_t dig_P8;
static int16_t dig_P9;
static int32_t t_fine;

static bool BMP280_write_register(uint8_t reg_addr, uint8_t val) {
    return i2c_write_register(&dev_handle, reg_addr, &val, 1);
}

static bool bmp280Read8(uint8_t reg, uint8_t *data) {
    return i2c_read_register(&dev_handle, reg, data, 1);
}

static bool bmp280Read16(uint8_t reg, uint16_t *data) {
    uint8_t buf[2];
    if (!i2c_read_register(&dev_handle, reg, buf, 2)) {
        return false;
    }
    *data = ((uint16_t)buf[0] << 8) | buf[1]; // MSB first
    return true;
}

static bool bmp280Read16LE(uint8_t reg, uint16_t *data) {
    uint8_t buf[2];
    if (!i2c_read_register(&dev_handle, reg, buf, 2)) {
        return false;
    }
    *data = ((uint16_t)buf[1] << 8) | buf[0]; // LSB first
    return true;
}

static bool bmp280ReadS16(uint8_t reg, int16_t *data) {
    uint16_t temp;
    if (!bmp280Read16(reg, &temp)) {
        return false;
    }
    *data = (int16_t)temp;
    return true;
}

static bool bmp280ReadS16LE(uint8_t reg, int16_t *data) {
    uint16_t temp;
    if (!bmp280Read16LE(reg, &temp)) {
        return false;
    }
    *data = (int16_t)temp;
    return true;
}

static bool bmp280Read24(uint8_t reg, uint32_t *data) {
    uint8_t buf[3];
    if (!i2c_read_register(&dev_handle, reg, buf, 3)) {
        return false;
    }
    *data = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    return true;
}

static bool BMP280_calc_altitude(const float p0, const float p1, const float t,
                          float *altitude) {

    float C;
    C = (p0 / p1);
    C = pow(C, (1.0f / 5.25588f)) - 1.0f;
    C = (C * (t + 273.15f)) / 0.0065f;
    *altitude = C;
    return true;
}


bool BMP280_init(void) {
    if (!i2c_create_device(&dev_handle, BMP280_ADDR))
        return false;

    uint8_t chip_id = 0;
    uint8_t retry = 0;
    while ((retry++ < 5) && (chip_id != 0x58)) {
        if (!bmp280Read8(BMP280_REG_CHIPID, &chip_id)) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    if (chip_id != 0x58) {
        return false; // chip ID validation failed
    }

    if (!bmp280Read16LE(BMP280_REG_DIG_T1, &dig_T1) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_T2, &dig_T2) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_T3, &dig_T3) ||
        !bmp280Read16LE(BMP280_REG_DIG_P1, &dig_P1) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P2, &dig_P2) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P3, &dig_P3) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P4, &dig_P4) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P5, &dig_P5) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P6, &dig_P6) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P7, &dig_P7) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P8, &dig_P8) ||
        !bmp280ReadS16LE(BMP280_REG_DIG_P9, &dig_P9)) {
        return false;
    }

    return BMP280_write_register(BMP280_REG_CONTROL, 0x3F);
}

bool BMP280_get_temperature(float *temp) {
    int32_t var1, var2;
    uint32_t adc_T;
    if (!bmp280Read24(BMP280_REG_TEMPDATA, &adc_T)) {
        return false;
    }

    adc_T >>= 4;
    var1 =
        (((adc_T >> 3) - ((int32_t)(dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) *
              ((adc_T >> 4) - ((int32_t)dig_T1))) >>
             12) *
            ((int32_t)dig_T3)) >>
           14;
    t_fine = var1 + var2;
    float T = (t_fine * 5 + 128) >> 8;

    *temp = T / 100;
    return true;
}
bool BMP280_get_pressure(uint32_t *pressure) {

    int64_t var1, var2, p;
    float t;
    if (!BMP280_get_temperature(&t)) {
        return false;
    }

    uint32_t adc_P;
    if (!bmp280Read24(BMP280_REG_PRESSUREDATA, &adc_P)) {
        return false;
    }
    adc_P >>= 4;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)dig_P6;
    var2 = var2 + ((var1 * (int64_t)dig_P5) << 17);
    var2 = var2 + (((int64_t)dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)dig_P3) >> 8) +
           ((var1 * (int64_t)dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)dig_P1) >> 33;
    if (var1 == 0) {
        return false; // avoid division by zero
    }
    p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)dig_P7) << 4);

    *pressure = (uint32_t)p / 256;
    return true;
}

bool BMP280_get_altitude(const float ref_pressure, float *altitude) {
    float t;
    uint32_t p;
    if (!BMP280_get_temperature(&t) || !BMP280_get_pressure(&p)) {
        return false;
    }

    return BMP280_calc_altitude(ref_pressure, p, t, altitude);
}

void BMP280_task(void *params) {
    if (BMP280_init()) {
        while (true) {
            float t;
            if (BMP280_get_temperature(&t)) {
                LOG_INFO(TAG, "Temperature: %f", t);
            } else {
                LOG_ERROR(TAG, "Failed to get temperature data");
            }
            printf("\n");

            uint32_t p;
            if (BMP280_get_pressure(&p)) {
                LOG_INFO(TAG, "Pressure: %d", p);
            } else {
                LOG_ERROR(TAG, "Failed to get pressure data");
            }
            printf("\n");

            float a;
            if (BMP280_get_altitude(STANDARD_SEA_LEVEL_PRESSURE, &a)) {
                LOG_INFO(TAG, "Altitude w standard sea level pressure as ref: %f", a);
            } else {
                LOG_ERROR(TAG, "Failed to get altitude data");
            }
            printf("\n");

            vTaskDelay(pdMS_TO_TICKS(500));
        }

    } else {
        LOG_ERROR(TAG, "Failed to init BMP280 barometer sensor");
    }

    vTaskDelete(NULL);
}

void run_BMP280_task(void) {

    xTaskCreate(BMP280_task,   // Task function
                "BMP280_task", // Task name
                4096,          // Stack size in words
                NULL,          // Task parameters
                5,             // Priority
                NULL           // Task handle
    );
}
