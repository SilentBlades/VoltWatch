# ==============================================================================
# VoltWatch - Root Build & Development Makefile
# ==============================================================================

PORT ?=
FQBN ?= esp32:esp32:esp32:PartitionScheme=min_spiffs
FLASH_BAUD ?= 460800
BAUD ?= 115200

.PHONY: all test build flash monitor clean help

help:
	@echo "VoltWatch Build System"
	@echo "--------------------------------------------------"
	@echo "make test     - Compile and run native C++ unit tests (no hardware needed)"
	@echo "make build    - Compile firmware using arduino-cli"
	@echo "make flash    - Upload firmware to connected ESP32 board (set PORT=/dev/... FLASH_BAUD=460800)"
	@echo "make monitor  - Connect to serial monitor at 115200 baud"
	@echo "make clean    - Remove build artifacts and test binaries"

test:
	@./scripts/run_tests.sh

build:
	@FQBN=$(FQBN) ./scripts/build.sh

flash:
	@PORT=$(PORT) FQBN=$(FQBN) FLASH_BAUD=$(FLASH_BAUD) ./scripts/flash.sh

monitor:
	@PORT=$(PORT) ./scripts/monitor.sh

clean:
	@rm -rf build bin
	@make -C test clean
