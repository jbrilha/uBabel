.PHONY: all clean pico esp32 pico-clean esp32-clean flash-pico flash-esp32 monitor-esp32 menu menuconfig m5core target-esp32 target-esp32s3 target-esp32c6

all: pico esp32

PICO_BUILD_DIR := build_pico
ESP32_BUILD_DIR := build_esp32

all-esp32: clean-esp32 esp32 flash-esp32 monitor-esp32
all-pico: clean-pico pico flash-pico

pico:
	@echo "Building for Pico..."
	@mkdir -p $(PICO_BUILD_DIR)
	cd $(PICO_BUILD_DIR) && cmake -DBUILD_PICO=1 .. && make
	ln -sf $(PICO_BUILD_DIR)/compile_commands.json compile_commands.json

esp32:
	@echo "Building for ESP32..."
	cp sdkconfig.esp32_defaults sdkconfig.esp32
	idf.py -B $(ESP32_BUILD_DIR) -DBUILD_ESP32=1 build
	ln -sf $(ESP32_BUILD_DIR)/compile_commands.json compile_commands.json

m5core:
	@echo "Building for M5Stack Core Basic..."
	cp sdkconfig.m5core sdkconfig.esp32
	idf.py -B $(ESP32_BUILD_DIR) -DBUILD_ESP32=1 -DM5STACK_CORE_BASIC=1 build
	ln -sf $(ESP32_BUILD_DIR)/compile_commands.json compile_commands.json

m5core-sender:
	@echo "Building for M5Stack Core Basic..."
	cp sdkconfig.m5core sdkconfig.esp32
	idf.py -B $(ESP32_BUILD_DIR) -DBUILD_ESP32=1 -DM5STACK_CORE_BASIC=1 -DM5STACK_SENDER=1 build
	ln -sf $(ESP32_BUILD_DIR)/compile_commands.json compile_commands.json

m5core-receiver:
	@echo "Building for M5Stack Core Basic..."
	cp sdkconfig.m5core sdkconfig.esp32
	idf.py -B $(ESP32_BUILD_DIR) -DBUILD_ESP32=1 -DM5STACK_CORE_BASIC=1 -DM5STACK_RECEIVER=1 build
	ln -sf $(ESP32_BUILD_DIR)/compile_commands.json compile_commands.json


clean: clean-pico clean-esp32

clean-pico:
	@echo "Cleaning Pico build..."
	rm -rf $(PICO_BUILD_DIR)

clean-esp32:
	@echo "Cleaning ESP32 build..."
	rm -rf $(ESP32_BUILD_DIR)

flash-pico:
	@echo "Flashing Pico..."
	picotool load -f $(PICO_BUILD_DIR)/main/micro-babel.uf2

flash-esp32:
	@echo "Flashing ESP32..."
	idf.py -B $(ESP32_BUILD_DIR) flash

monitor-esp32:
	@echo "Starting ESP32 monitor..."
	idf.py monitor

menu: menuconfig

menuconfig:
	idf.py -B $(ESP32_BUILD_DIR) menuconfig

target-esp32:
	idf.py -B $(ESP32_BUILD_DIR) set-target esp32

target-esp32s3:
	idf.py -B $(ESP32_BUILD_DIR) set-target esp32s3

target-esp32c6:
	idf.py -B $(ESP32_BUILD_DIR) set-target esp32c6
