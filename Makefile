# ==============================================================================
# VoltWatch - Root Monorepo Makefile (Forwarding Targets)
# ==============================================================================

PORT ?=
FQBN ?= esp32:esp32:esp32:PartitionScheme=min_spiffs
BAUD ?= 115200
FLASH_BAUD ?= 460800

.PHONY: all test build flash monitor clean help

all: build

help:
	@echo "VoltWatch Monorepo"
	@echo "--------------------------------------------------"
	@echo "make test     - Run unit tests in firmware/ (or cd firmware && make test)"
	@echo "make build    - Compile headless ESP32 firmware in firmware/"
	@echo "make flash    - Flash firmware to ESP32 (PORT=/dev/... FLASH_BAUD=460800)"
	@echo "make monitor  - Open serial monitor (PORT=/dev/... BAUD=115200)"
	@echo "make clean    - Clean firmware build artifacts"
	@echo "--------------------------------------------------"
	@echo "Web SPA: Open web/index.html in browser or host via GitHub Pages"

test:
	@$(MAKE) -C firmware test

build:
	@$(MAKE) -C firmware build PORT=$(PORT) FQBN=$(FQBN)

flash:
	@$(MAKE) -C firmware flash PORT=$(PORT) FQBN=$(FQBN) FLASH_BAUD=$(FLASH_BAUD)

monitor:
	@$(MAKE) -C firmware monitor PORT=$(PORT) BAUD=$(BAUD)

clean:
	@$(MAKE) -C firmware clean
	@rm -rf build bin
