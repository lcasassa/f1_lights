.PHONY: help build upload monitor clean build-test upload-test monitor-test sim sim-clean

# Auto-discover USB port (can override with: make upload PORT=/dev/cu.usbserial-XXXX)
PORT ?= $(shell ls /dev/cu.usbserial-* 2>/dev/null | head -n 1)

# Fallback if no port found
ifeq ($(PORT),)
PORT_ERROR = "No Arduino found. Connect your board and try again, or specify: make upload PORT=/dev/cu.usbserial-XXXX"
endif

help:
	@echo "Arduino Pro Mini F1 Lights - Makefile targets:"
	@echo ""
	@echo "F1 START SEQUENCE MODE (default):"
	@echo "  make build             - Compile F1 sequence firmware"
	@echo "  make upload            - Build and upload F1 sequence (auto-discovers port)"
	@echo "  make monitor           - Open serial monitor (auto-discovers port)"
	@echo ""
	@echo "HARDWARE TEST MODE:"
	@echo "  make build-test        - Compile hardware test firmware"
	@echo "  make upload-test       - Build and upload test firmware (auto-discovers port)"
	@echo "  make monitor-test      - Open serial monitor for test mode"
	@echo ""
	@echo "DESKTOP SIMULATION (Python):"
	@echo "  make sim               - Build shared library for Python simulation"
	@echo "  make sim-clean         - Remove simulation build artifacts"
	@echo ""
	@echo "UTILITY:"
	@echo "  make clean             - Remove build artifacts"


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

all: build upload
	@echo "Done! Open another terminal to monitor:"
	@echo "  make monitor PORT=$(PORT)"

clean:
	@echo "Cleaning build artifacts..."
	pio run -t clean
	rm -rf .pio build/
	@echo "Clean complete."

# ── Desktop simulation (shared library for Python ctypes) ───────────────────
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
  SIM_LIB = libf1sim.dylib
  SIM_FLAGS = -dynamiclib
else
  SIM_LIB = libf1sim.so
  SIM_FLAGS = -shared
endif

sim: $(SIM_LIB)

$(SIM_LIB): sim/arduino_stub.cpp sim/sim_bridge.cpp src/main.cpp sim/Arduino.h
	@echo "Building simulation shared library ($(SIM_LIB))..."
	c++ -std=c++17 -O2 -fPIC $(SIM_FLAGS) -I sim sim/arduino_stub.cpp sim/sim_bridge.cpp src/main.cpp -o $(SIM_LIB)
	@echo "Built $(SIM_LIB) — use from Python:  from f1_sim import F1Sim"

sim-clean:
	rm -f libf1sim.dylib libf1sim.so
	@echo "Simulation artifacts removed."

.DEFAULT_GOAL := help

