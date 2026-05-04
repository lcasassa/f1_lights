.PHONY: help build upload monitor clean build-test upload-test monitor-test sim sim-clean test display wasm web web-dev build-screen upload-screen build-screen-diag upload-screen-diag build-screen-dino upload-screen-dino build-esp32 upload-esp32 monitor-esp32 upload-esp32-ota upload-esp32-ota-full upload-esp32-segscan

# Auto-discover USB port (can override with: make upload PORT=/dev/cu.usbserial-XXXX)
PORT ?= $(shell ls /dev/cu.usbserial-* 2>/dev/null | head -n 1)

# Fallback if no port found
ifeq ($(PORT),)
PORT_ERROR = "No Arduino found. Connect your board and try again, or specify: make upload PORT=/dev/cu.usbserial-XXXX"
endif

# ESP32-C3 Super Mini exposes a native USB CDC port (usbmodem*)
ESP32_PORT ?= $(shell ls /dev/cu.usbmodem* 2>/dev/null | head -n 1)
ESP32_PORT_ERROR = "No ESP32-C3 found. Connect the board (hold BOOT if needed) or specify: make upload-esp32 ESP32_PORT=/dev/cu.usbmodemXXXX"

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
	@echo "WS2812B SCREEN TEST:"
	@echo "  make build-screen      - Compile WS2812B matrix test firmware"
	@echo "  make upload-screen     - Build and upload screen test firmware"
	@echo "  make build-screen-dino - Compile Dino game firmware"
	@echo "  make upload-screen-dino- Build and upload Dino game firmware"
	@echo ""
	@echo "ESP32-C3 SUPER MINI:"
	@echo "  make build-esp32       - Compile ESP32-C3 blink firmware"
	@echo "  make upload-esp32      - Build and upload ESP32-C3 blink firmware"
	@echo "  make monitor-esp32     - Open serial monitor for ESP32-C3"
	@echo "  make upload-esp32-ota  - Push firmware over WiFi (lean: no GitHub OTA)"
	@echo "  make upload-esp32-ota-full - Push firmware over WiFi (incl. GitHub OTA)"
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

build-screen:
	@echo "Building WS2812B screen test firmware..."
	pio run -e screen

upload-screen: build-screen
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Uploading screen test firmware to $(PORT)..."
	pio run -e screen -t upload --upload-port $(PORT)

build-screen-diag:
	@echo "Building WS2812B LED chain diagnostic..."
	pio run -e screen-diag

upload-screen-diag: build-screen-diag
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Uploading LED chain diagnostic to $(PORT)..."
	pio run -e screen-diag -t upload --upload-port $(PORT)

build-screen-dino:
	@echo "Building Dino game firmware..."
	pio run -e screen-dino

upload-screen-dino: build-screen-dino
	@if [ -z "$(PORT)" ]; then echo $(PORT_ERROR); exit 1; fi
	@echo "Uploading Dino game firmware to $(PORT)..."
	pio run -e screen-dino -t upload --upload-port $(PORT)

# ── ESP32-C3 Super Mini ─────────────────────────────────────────────────────
# Inject the local short SHA into every ESP32 build so the on-device version
# display (and the GitHub OTA-check) shows something sensible instead of
# the "dev" placeholder. CI overrides this with the canonical commit SHA.
ESP32_FW_VERSION := $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
export PLATFORMIO_BUILD_FLAGS = -DFIRMWARE_VERSION=\"$(ESP32_FW_VERSION)\"

build-esp32:
	@echo "Building ESP32-C3 Super Mini blink firmware (fw=$(ESP32_FW_VERSION))..."
	pio run -e esp32-c3-supermini

upload-esp32: build-esp32
	@if [ -z "$(ESP32_PORT)" ]; then echo $(ESP32_PORT_ERROR); exit 1; fi
	@echo "Uploading ESP32-C3 firmware to $(ESP32_PORT)..."
	pio run -e esp32-c3-supermini -t upload --upload-port $(ESP32_PORT)

monitor-esp32:
	@if [ -z "$(ESP32_PORT)" ]; then echo $(ESP32_PORT_ERROR); exit 1; fi
	@echo "Opening serial monitor on $(ESP32_PORT) at 115200 baud..."
	pio device monitor --baud 115200 --port $(ESP32_PORT) --echo

# Push firmware to the ESP32-C3 over WiFi via ArduinoOTA / mDNS.
#
# Two flavors:
#   make upload-esp32-ota          → lean image (no GitHub self-updater),
#                                    fastest dev iteration.
#   make upload-esp32-ota-full     → full image incl. GitHub self-updater.
#
# Speed tips:
#   - Override OTA_HOST with the device's IP to skip the mDNS lookup
#     (saves 1–3 s on macOS):     make upload-esp32-ota OTA_HOST=192.168.1.42
#   - The lean env (default) ships ~100–150 KB less binary, so the WiFi
#     transfer + flash-write step is ~15–20 % faster on top of that.
OTA_HOST ?= f1-esp32.local

# Resolve mDNS once on the make host (dns-sd is built-in on macOS); falls
# back silently to letting espota.py do the lookup if resolution fails.
# Override by passing OTA_HOST=<ip> on the command line.
ifeq ($(OTA_HOST),f1-esp32.local)
  RESOLVED_OTA_HOST := $(shell dns-sd -t 2 -G v4 $(OTA_HOST) 2>/dev/null \
      | awk '/IPv4/ {print $$6; exit}')
  ifneq ($(RESOLVED_OTA_HOST),)
    OTA_HOST := $(RESOLVED_OTA_HOST)
  endif
endif

upload-esp32-ota:
	@echo "OTA upload (lean, fw=$(ESP32_FW_VERSION)) to $(OTA_HOST) ..."
	pio run -e esp32-c3-supermini-ota-fast -t upload --upload-port $(OTA_HOST)

upload-esp32-ota-full:
	@echo "OTA upload (full, fw=$(ESP32_FW_VERSION), w/ GitHub self-updater) to $(OTA_HOST) ..."
	pio run -e esp32-c3-supermini-ota -t upload --upload-port $(OTA_HOST)

# Flash the segment-bit scanner over USB: lights one HT16K33 bit at a time
# on Dig1 so you can identify which physical segment each bit drives.
# Watch the panel and `make monitor-esp32` simultaneously to answer the
# binary-search prompts (y/n/r/q).
upload-esp32-segscan:
	@if [ -z "$(ESP32_PORT)" ]; then echo $(ESP32_PORT_ERROR); exit 1; fi
	@echo "Segment-scan upload (USB) to $(ESP32_PORT) ..."
	pio run -e esp32-c3-supermini-segscan -t upload --upload-port $(ESP32_PORT)

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
	c++ -std=c++17 -O2 -fPIC $(SIM_FLAGS) -I $(SIM_CSRC) -I src $(SIM_CSRC)/arduino_stub.cpp $(SIM_CSRC)/sim_bridge.cpp src/main.cpp -o $(SIM_LIB)
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
WASM_SOURCES = $(SIM_CSRC)/arduino_stub.cpp $(SIM_CSRC)/sim_bridge.cpp src/main.cpp $(SIM_CSRC)/Arduino.h $(SIM_CSRC)/Wire.h src/ht16k33_display.h

wasm: $(WASM_OUT)

$(WASM_OUT): $(WASM_SOURCES)
	@mkdir -p web/public
	@echo "Building WASM module..."
	emcc -std=c++17 -O2 \
		-I $(SIM_CSRC) \
		-I src \
		$(SIM_CSRC)/arduino_stub.cpp \
		$(SIM_CSRC)/sim_bridge.cpp \
		src/main.cpp \
		-s EXPORTED_FUNCTIONS='["_sim_setup","_sim_loop","_sim_set_millis","_sim_get_millis","_sim_advance_millis","_sim_get_pin_value","_sim_set_pin_input","_sim_reset","_sim_get_tone_freq","_sim_get_tone_pin","_sim_get_tone_log_count","_sim_get_tone_log_ms","_sim_get_tone_log_freq","_sim_tone_log_clear","_sim_get_display_buf","_sim_get_display_byte","_malloc","_free"]' \
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

