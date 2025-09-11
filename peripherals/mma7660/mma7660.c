#include "mma7660.h"
#include "i2c_hal.h"
#include "platform.h"

static mma7660_lookup_t acc_lookup[64];

static i2c_device_handle_t device;

static const char *TAG = "MMA7660";

#define MAX_RETRIES 3

static bool read_reg(uint8_t reg_addr, uint8_t *val) {
    return i2c_read_register(&device, reg_addr, val, 1);
}

static bool write_reg(uint8_t reg_addr, uint8_t val) {
    return i2c_write_register(&device, reg_addr, &val, 1);
}

static void init_accel_table() {
    int i;
    float val, valZ;

    for (i = 0, val = 0; i < 32; i++) {
        acc_lookup[i].g = val;
        val += 0.047;
    }

    for (i = 63, val = -0.047; i > 31; i--) {
        acc_lookup[i].g = val;
        val -= 0.047;
    }

    for (i = 0, val = 0, valZ = 90; i < 22; i++) {
        acc_lookup[i].xyAngle = val;
        acc_lookup[i].zAngle = valZ;

        val += 2.69;
        valZ -= 2.69;
    }

    for (i = 63, val = -2.69, valZ = -87.31; i > 42; i--) {
        acc_lookup[i].xyAngle = val;
        acc_lookup[i].zAngle = valZ;

        val -= 2.69;
        valZ += 2.69;
    }

    for (i = 22; i < 43; i++) {
        acc_lookup[i].xyAngle = 255;
        acc_lookup[i].zAngle = 255;
    }
}

static void set_mode(uint8_t mode) { write_reg(MMA7660_MODE, mode); }
static void set_sample_rate(uint8_t rate) { write_reg(MMA7660_SR, rate); }

bool MMA7660_init(void) {
    if (!i2c_create_device(&device, MMA7660_ADDR)) {
        return false;
    }

    init_accel_table();

    set_mode(MMA7660_STAND_BY);
    set_sample_rate(AUTO_SLEEP_32);
    set_mode(MMA7660_ACTIVE);

    return true;
}

bool MMA7660_get_xyz(int8_t *x, int8_t *y, int8_t *z) {
    uint8_t val[3];
    int retry_count = 0;

START:
    if (!i2c_read_register(&device, 0x00, val, 3)) {
        if (retry_count++ < MAX_RETRIES) {
            goto START;
        }
        return false;
    }

    for (int i = 0; i < 3; i++) {
        if (val[i] > 63) {
            if (retry_count++ < MAX_RETRIES) {
                goto START;
            }
            return false;
        }
    }

    *x = ((int8_t)(val[0] << 2)) / 4;
    *y = ((int8_t)(val[1] << 2)) / 4;
    *z = ((int8_t)(val[2] << 2)) / 4;

    return true;
}

bool MMA7660_get_acceleration_raw(float *ax, float *ay, float *az) {
    int8_t x, y, z;
    if (!MMA7660_get_xyz(&x, &y, &z)) {
        return false;
    }

    *ax = x / 21.00;
    *ay = y / 21.00;
    *az = z / 21.00;

    return true;
}

bool MMA7660_get_acceleration(mma7660_acc_data_t *data) {
    uint8_t val[3];
    int retry_count = 0;

START:
    if (!i2c_read_register(&device, 0x00, val, 3)) {
        if (retry_count++ < MAX_RETRIES) {
            goto START;
        }
        return false;
    }

    for (int i = 0; i < 3; i++) {
        if (val[i] & 0x40) { // alert bit set, data is garbage
            if (retry_count++ < MAX_RETRIES) {
                goto START;
            }
            return false;
        }
    }

    data->x = acc_lookup[val[0] & 0x3F];
    data->y = acc_lookup[val[1] & 0x3F];
    data->z = acc_lookup[val[2] & 0x3F];

    return true;
}
bool MMA7660_get_all_data(mma7660_data_t *data) {
    uint8_t val[11];

    if (!i2c_read_register(&device, 0x00, val, 11)) {
        return false;
    }

    data->X = val[0];
    data->Y = val[1];
    data->Z = val[2];
    data->TILT = val[3];
    data->SRST = val[4];
    data->SPCNT = val[5];
    data->INTSU = val[6];
    data->MODE = val[7];
    data->SR = val[8];
    data->PDET = val[9];
    data->PD = val[10];

    return true;
}

void mma7660_task(void *params) {
    if (MMA7660_init()) {
        while (true) {
            int8_t x, y, z;
            if (MMA7660_get_xyz(&x, &y, &z)) {
                LOG_INFO(TAG, "X: %d, Y: %d, Z: %d", x, y, z);
            } else {
                LOG_ERROR(TAG, "Failed to get XYZ data");
            }
            printf("\n");

            float ax, ay, az;
            if (MMA7660_get_acceleration_raw(&ax, &ay, &az)) {
                LOG_INFO(TAG, "Acc X: %f, aY: %f, aZ: %f", ax, ay, az);
            } else {
                LOG_ERROR(TAG, "Failed to get raw acceleration data");
            }
            printf("\n");

            mma7660_acc_data_t acc_data;
            if (MMA7660_get_acceleration(&acc_data)) {
                LOG_INFO(TAG, "Acc X: g=%.3f, xy=%.1f°, z=%.1f°", acc_data.x.g,
                         acc_data.x.xyAngle, acc_data.x.zAngle);
                LOG_INFO(TAG, "Acc Y: g=%.3f, xy=%.1f°, z=%.1f°", acc_data.y.g,
                         acc_data.y.xyAngle, acc_data.y.zAngle);
                LOG_INFO(TAG, "Acc Z: g=%.3f, xy=%.1f°, z=%.1f°", acc_data.z.g,
                         acc_data.z.xyAngle, acc_data.z.zAngle);
            } else {
                LOG_ERROR(TAG, "Failed to get acceleration data");
            }
            printf("\n");

            mma7660_data_t all_data;
            if (MMA7660_get_all_data(&all_data)) {
                LOG_INFO(TAG, "Raw: X=0x%02X, Y=0x%02X, Z=0x%02X", all_data.X,
                         all_data.Y, all_data.Z);
                LOG_INFO(TAG, "TILT=0x%02X, SRST=0x%02X, SPCNT=0x%02X",
                         all_data.TILT, all_data.SRST, all_data.SPCNT);
                LOG_INFO(TAG, "INTSU=0x%02X, MODE=0x%02X, SR=0x%02X",
                         all_data.INTSU, all_data.MODE, all_data.SR);
                LOG_INFO(TAG, "PDET=0x%02X, PD=0x%02X", all_data.PDET,
                         all_data.PD);
            } else {
                LOG_ERROR(TAG, "Failed to get all data");
            }
            printf("\n");

            vTaskDelay(pdMS_TO_TICKS(500));
        }

    } else {
        LOG_ERROR(TAG, "Failed to init MMA7660 accelerometer");
    }

    vTaskDelete(NULL);
}

void run_mma7660_task(void) {

    xTaskCreate(mma7660_task,   // Task function
                "mma7660_task", // Task name
                4096,           // Stack size in words
                NULL,           // Task parameters
                5,              // Priority
                NULL            // Task handle
    );
}
