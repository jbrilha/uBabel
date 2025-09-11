#include "tca9548.h"
#include "i2c_hal.h"

static i2c_device_handle_t dev_handle;
static tca_channel_t channels;

static bool TCA9548_write(uint8_t chans) {
    return i2c_write_bytes(&dev_handle, &chans, 1);
}

bool TCA9548_init(void) { return i2c_create_device(&dev_handle, TCA9548_ADDR); }

bool TCA9548_open_channel(tca_channel_t channel) {
    channels |= channel;
    return TCA9548_write(channels);
}

bool TCA9548_close_channel(tca_channel_t channel) {
    channels ^= channel;
    return TCA9548_write(channels);
}

bool TCA9548_open_all_channels(void) {
    channels = 0xFF;
    return TCA9548_write(channels);
}

bool TCA9548_close_all_channels(void) {
    channels = 0x00;
    return TCA9548_write(channels);
}
