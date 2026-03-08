# Micro-Babel Core

## Setup

### ESP-IDF

```bash
mkdir -p ~/esp
cd ~/esp

git clone -b v5.5 --depth=1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

./install.sh all

. ./export.sh

# RECOMMENDED
echo "alias get_idf='. $HOME/esp/esp-idf/export.sh'" >> ~/.bashrc # or ~/.zshrc, etc.
```

### PICO-SDK

```bash
mkdir ~/pico
cd ~/pico

git clone --depth=1 https://github.com/raspberrypi/pico-sdk.git --branch master
cd pico-sdk

git submodule update --init

echo "export PICO_SDK_PATH=~/pico/pico-sdk/" >> ~/.bashrc # or ~/.zshrc, etc.
```

`picotool` is not mandatory but highly recommended (especially to use the Makefile).
It's likely available in your OS's package manager, but installation instructions can be found [here](https://github.com/raspberrypi/picotool?tab=readme-ov-file#building--installing).

### FreeRTOS (to use with pico-sdk)

```bash
mkdir ~/FreeRTOS/
cd ~/FreeRTOS

git clone --depth=1 https://github.com/raspberrypi/FreeRTOS-Kernel.git

echo "export FREERTOS_KERNEL_PATH=~/FreeRTOS/FreeRTOS-Kernel/" >> ~/.bashrc # or ~/.zshrc, etc.
```

## Building & Flashing

We use two separate build directories for the ESP32 and Pico, so it's recommended to use the `Makefile`.

### For ESP32

```bash
get_idf # if you set the alias earlier
make clean-esp32
make target-<chip> # esp32, esp32s3, etc.
make build-esp32
make flash-esp32
```

### For Pico W

```bash
make clean-pico
make build-pico
make flash-pico
```
