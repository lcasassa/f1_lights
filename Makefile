.PHONY: help build upload monitor clean build-test upload-test monitor-test build-ht16k33 upload-ht16k33 monitor-ht16k33 sim sim-clean test display wasm web web-dev

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
	@echo "HT16K33 BLINK TEST:"
	@echo "  make build-ht16k33    - Compile HT16K33 blink test firmware"
	@echo "  make upload-ht16k33   - Build and upload HT16K33 blink test"
	@echo "  make monitor-ht16k33  - Open serial monitor for HT16K33 test"
	@echo ""
	@echo "DESKTOP SIMULATION (Python):"
	@echo "  make sim               - Build shared library for Python simulation"
	@echo "  make test              - Build sim library and run pytest"
	@echo "  make display           - Launch pygame visualiser (keys: 1=left 2=right)"
	@echo "  make sim-clean         - Remove simulation build artifacts"
	@echo ""
	@echo "WEB SIMULATOR (Vue + WASM):"
	@echo "  make web-dev           - Start Vite dev server (hot-reload)"
	@echo "  make web               - Build for production (web/dist/)"
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
	@echo "Opening serial monitor on $(PORT) at 115200 baud..."
	pio device monitor --baud 115200 --port $(PORT) --echo

monitor-test:
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Opening serial monitor (TEST mode) on $(PORT) at 115200 baud..."
	pio device monitor --baud 115200 --port $(PORT) --echo

build-ht16k33:
	@echo "Building HT16K33 blink test firmware..."
	pio run -e ht16k33-test

upload-ht16k33: build-ht16k33
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Uploading HT16K33 blink test to $(PORT)..."
	pio run -e ht16k33-test -t upload --upload-port $(PORT)

monitor-ht16k33:
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Opening serial monitor (HT16K33 test) on $(PORT) at 115200 baud..."
	pio device monitor --baud 115200 --port $(PORT) --echo

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
  SIM_LIB = build/libf1sim.dylib
  SIM_FLAGS = -dynamiclib
else
  SIM_LIB = build/libf1sim.so
  SIM_FLAGS = -shared
endif

SIM_CSRC = f1_sim/csrc

sim: $(SIM_LIB)

$(SIM_LIB): $(SIM_CSRC)/arduino_stub.cpp $(SIM_CSRC)/sim_bridge.cpp src/main.cpp $(SIM_CSRC)/Arduino.h
	@mkdir -p build
	@echo "Building simulation shared library ($(SIM_LIB))..."
	c++ -std=c++17 -O2 -fPIC $(SIM_FLAGS) -I $(SIM_CSRC) $(SIM_CSRC)/arduino_stub.cpp $(SIM_CSRC)/sim_bridge.cpp src/main.cpp -o $(SIM_LIB)
	@echo "Built $(SIM_LIB) — use from Python:  from f1_sim import F1Sim"

sim-clean:
	rm -f build/libf1sim.dylib build/libf1sim.so
	@echo "Simulation artifacts removed."

test: sim
	python -m pytest tests/ -v

display: sim
	python -m f1_sim.f1_display

# ── Web simulator (Vue + WASM) ──────────────────────────────────────────────
WASM_OUT = web/public/f1sim.js
WASM_SOURCES = $(SIM_CSRC)/arduino_stub.cpp $(SIM_CSRC)/sim_bridge.cpp src/main.cpp $(SIM_CSRC)/Arduino.h

wasm: $(WASM_OUT)

$(WASM_OUT): $(WASM_SOURCES)
	@mkdir -p web/public
	@echo "Building WASM module..."
	emcc -std=c++17 -O2 \
		-I $(SIM_CSRC) \
		$(SIM_CSRC)/arduino_stub.cpp \
		$(SIM_CSRC)/sim_bridge.cpp \
		src/main.cpp \
		-s EXPORTED_FUNCTIONS='["_sim_setup","_sim_loop","_sim_set_millis","_sim_get_millis","_sim_advance_millis","_sim_get_pin_value","_sim_set_pin_input","_sim_reset","_sim_get_tone_freq","_sim_get_tone_pin","_sim_get_tone_log_count","_sim_get_tone_log_ms","_sim_get_tone_log_freq","_sim_tone_log_clear"]' \
		-s EXPORTED_RUNTIME_METHODS='["ccall","cwrap"]' \
		-s MODULARIZE=1 \
		-s EXPORT_NAME='createF1Sim' \
		-s ENVIRONMENT=web \
		-s ALLOW_MEMORY_GROWTH=0 \
		-o $(WASM_OUT)
	@echo "Built $(WASM_OUT)"

web-dev: wasm
	cd web && npm install --silent && npx vite

web: wasm
	cd web && npm install --silent && npx vite build

.DEFAULT_GOAL := help

