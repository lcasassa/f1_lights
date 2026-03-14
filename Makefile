.PHONY: help simulate build upload monitor clean build-test upload-test monitor-test

# Auto-discover USB port (can override with: make upload PORT=/dev/cu.usbserial-XXXX)
PORT ?= $(shell ls /dev/cu.usbserial-* 2>/dev/null | head -n 1)

# Fallback if no port found
ifeq ($(PORT),)
PORT_ERROR = "No Arduino found. Connect your board and try again, or specify: make upload PORT=/dev/cu.usbserial-XXXX"
endif

help:
	@echo "Arduino Pro Mini Blink - Makefile targets:"
	@echo ""
	@echo "F1 START SEQUENCE MODE (default):"
	@echo "  make simulate  - Run local blink timing preview"
	@echo "  make build     - Compile F1 sequence firmware"
	@echo "  make upload    - Build and upload F1 sequence (auto-discovers port)"
	@echo "  make monitor   - Open serial monitor (auto-discovers port)"
	@echo "  make all       - Simulate, build, and upload F1 sequence"
	@echo ""
	@echo "HARDWARE TEST MODE:"
	@echo "  make build-test   - Compile hardware test firmware"
	@echo "  make upload-test  - Build and upload test firmware (auto-discovers port)"
	@echo "  make monitor-test - Open serial monitor for test mode"
	@echo ""
	@echo "UTILITY:"
	@echo "  make clean     - Remove build artifacts"

simulate:
	@echo "Running blink timing simulation..."
	python3 tools/blink_preview.py

build:
	@echo "Building firmware for Arduino Pro Mini (5V, 16MHz)..."
	pio run

build-test:
	@echo "Building TEST firmware for Arduino Pro Mini (5V, 16MHz)..."
	pio run -e pro16MHzatmega328-test

upload: build
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Uploading firmware to $(PORT)..."
	pio run -t upload --upload-port $(PORT)

upload-test: build-test
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Uploading TEST firmware to $(PORT)..."
	pio run -e pro16MHzatmega328-test -t upload --upload-port $(PORT)

monitor:
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Opening serial monitor on $(PORT) at 9600 baud..."
	pio device monitor --baud 9600 --port $(PORT)

monitor-test:
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Opening serial monitor (TEST mode) on $(PORT) at 9600 baud..."
	pio device monitor --baud 9600 --port $(PORT)

all: simulate build upload
	@echo "Done! Open another terminal to monitor:"
	@echo "  make monitor PORT=$(PORT)"

clean:
	@echo "Cleaning build artifacts..."
	pio run -t clean
	rm -rf .pio build/
	@echo "Clean complete."

.DEFAULT_GOAL := help

