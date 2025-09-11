#ifndef MMA7660_H
#define MMA7660_H

#include <stdbool.h>
#include <stdint.h>

#define MMA7660_ADDR 0x4C

#define MMA7660_X 0x00
#define MMA7660_Y 0x01
#define MMA7660_Z 0x02
#define MMA7660_TILT 0x03
#define MMA7660_SRST 0x04
#define MMA7660_SPCNT 0x05
#define MMA7660_INTSU 0x06
#define MMA7660_SHINTX 0x80
#define MMA7660_SHINTY 0x40
#define MMA7660_SHINTZ 0x20
#define MMA7660_GINT 0x10
#define MMA7660_ASINT 0x08
#define MMA7660_PDINT 0x04
#define MMA7660_PLINT 0x02
#define MMA7660_FBINT 0x01
#define MMA7660_MODE 0x07
#define MMA7660_STAND_BY 0x00
#define MMA7660_ACTIVE 0x01
#define MMA7660_SR 0x08     // sample rate register
#define AUTO_SLEEP_120 0X00 // 120 sample per second
#define AUTO_SLEEP_64 0X01
#define AUTO_SLEEP_32 0X02
#define AUTO_SLEEP_16 0X03
#define AUTO_SLEEP_8 0X04
#define AUTO_SLEEP_4 0X05
#define AUTO_SLEEP_2 0X06
#define AUTO_SLEEP_1 0X07
#define MMA7660_PDET 0x09
#define MMA7660_PD 0x0A

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
