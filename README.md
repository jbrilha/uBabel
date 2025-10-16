# Micro-Babel Core

## Setup

### ESP-IDF

``` bash
mkdir -p esp
cd ~/esp

git clone -b v5.5 --depth=1 --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

./install.sh all

. ./export.sh

# RECOMMENDED
echo "alias get_idf='. $HOME/esp/esp-idf/export.sh'" >> ~/.bashrc
# you MUST do this every time you open a terminal session to build for the ESPs otherwise the Makefile will NOT work
```

### PICO-SDK

``` bash
mkdir ~/pico
cd ~/pico

git clone --depth=1 https://github.com/raspberrypi/pico-sdk.git --branch master
cd pico-sdk

git submodule update --init

echo "export PICO_SDK_PATH=~/pico/pico-sdk/" >> ~/.bashrc
```

Also recommend installing ```picotool```

### FreeRTOS (to use with pico-sdk)

``` bash
mkdir ~/FreeRTOS/
cd ~/FreeRTOS

git clone --depth=1 https://github.com/raspberrypi/FreeRTOS-Kernel.git

echo "export FREERTOS_KERNEL_PATH=~/FreeRTOS/FreeRTOS-Kernel/" >> ~/.bashrc
```

## Building

### For ESP32

``` bash
# get_idf
idf.py set-target esp32 # or esp32s3, etc
idf.py build
idf.py flash # idf.py -p PORT flash
idf.py monitor # idf.py -p PORT monitor
```

or

``` bash
# get_idf
make clean-esp32
idf.py set-target esp32 # or esp32s3, etc
make esp32
make flash-esp32
```

### For Pico W 
``` bash
rm -rf build
mkdir build
cd build
cmake ..
make

picotool load -f main/micro-babel.uf2
```

or

``` bash
make clean-pico
make pico
make flash-pico
```
