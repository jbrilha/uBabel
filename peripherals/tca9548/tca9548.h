#ifndef TCA9548_H
#define TCA9548_H

#include <stdbool.h>

#define TCA9548_ADDR 0x70

typedef enum {
    TCA_CHANNEL_0 = 0x1,
    TCA_CHANNEL_1 = 0x2,
    TCA_CHANNEL_2 = 0x4,
    TCA_CHANNEL_3 = 0x8,
    TCA_CHANNEL_4 = 0x10,
    TCA_CHANNEL_5 = 0x20,
    TCA_CHANNEL_6 = 0x40,
    TCA_CHANNEL_7 = 0x80
} tca_channel_t;

bool TCA9548_init(void);

bool TCA9548_open_channel(tca_channel_t channel);
bool TCA9548_close_channel(tca_channel_t channel);

bool TCA9548_open_all_channels(void);
bool TCA9548_close_all_channels(void);

#endif // !TCA9548_H
