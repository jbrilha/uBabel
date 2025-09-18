#ifndef MMA7660_H
#define MMA7660_H

#include <stdbool.h>
#include <stdint.h>

#define MMA7660_ADDR 0x4C

typedef struct {
    uint8_t X;
    uint8_t Y;
    uint8_t Z;
    uint8_t TILT;
    uint8_t SRST;
    uint8_t SPCNT;
    uint8_t INTSU;
    uint8_t MODE;
    uint8_t SR;
    uint8_t PDET;
    uint8_t PD;
} mma7660_data_t;

typedef struct {
    float g;
    float xyAngle;
    float zAngle;
} mma7660_lookup_t;

typedef struct {
    mma7660_lookup_t x;
    mma7660_lookup_t y;
    mma7660_lookup_t z;
} mma7660_acc_data_t;

bool MMA7660_init(void);

bool MMA7660_get_xyz(int8_t *x, int8_t *y, int8_t *z);
bool MMA7660_get_acceleration_raw(float *ax, float *ay, float *az);
bool MMA7660_get_acceleration(mma7660_acc_data_t *data);
bool MMA7660_get_all_data(mma7660_data_t *data);

void run_mma7660_task(void);

#endif // !MMA7660_H
