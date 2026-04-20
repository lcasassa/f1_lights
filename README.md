# F1 Start Lights

Two-player reaction-time game built on an **Arduino Pro Mini 5V 16MHz**.
Ten LEDs (two rows of five) and two 4-digit 7-segment displays (driven by an
HT16K33 I2C backpack) reproduce the Formula 1 start-light sequence; a passive
buzzer adds engine-startup and race sounds.  Reaction times are shown on the
seven-segment displays after each round.

## How to play

1. Press any button to enter READY mode (border animation on 7-segment display).
2. Once both players are ready, five pairs of LEDs light up at 800 ms intervals (with a beep for each).
3. After a random 1–4 second pause, **lights out — GO!**
4. First player to press wins; their LED row lights up and reaction times are displayed.
5. Pressing *before* lights-out is a **jump start** — the other player wins automatically with dashes shown.
6. If both press within 100 ms of each other it's a **tie** and all LEDs light up.
7. After a result, press any button to return to READY for the next round.

## Project structure

| Path | Description |
|---|---|
| `platformio.ini` | PlatformIO board/framework config (default + test environments) |
| `src/main.cpp` | F1 start-sequence game firmware (HT16K33 + direct LEDs) |
| `src/ht16k33_display.h` | HT16K33 7-segment display + LED driver class |
| `src/main_test.cpp` | Hardware test firmware (sequential LED sweep) |
| `f1_sim/` | Python simulation package (ctypes wrapper + pygame display) |
| `f1_sim/csrc/` | C++ stubs and bridge for desktop/WASM simulation |
| `tests/` | Pytest suite exercising the full game via the simulation |
| `web/` | Vue 3 + Vite browser simulator (WASM) |
| `Makefile` | Build automation with auto-port detection |
| `pyproject.toml` | Python project metadata and dependencies |

## Quick start

### Using Makefile (recommended)

```zsh
# See all available targets
make help

# Compile firmware
make build

# Upload to board (auto-discovers USB port)
make upload

# Open serial monitor
make monitor

# Clean build artifacts
make clean

# Manual port override (if auto-discovery fails)
make upload PORT=/dev/cu.usbserial-XXXX
make monitor PORT=/dev/cu.usbserial-XXXX
```

### Hardware test mode

```zsh
make build-test     # compile test firmware
make upload-test    # upload test firmware
make monitor-test   # serial monitor for test mode
```

### Manual PlatformIO commands

```zsh
pio run
pio run -t upload --upload-port /dev/cu.usbserial-XXXX
pio device monitor --baud 9600 --port /dev/cu.usbserial-XXXX
```

## Desktop simulation (Python)

The C++ firmware compiles into a shared library so the full game logic can run
on your desktop and be driven from Python.

**Prerequisites:** Python ≥ 3.9, a C++17 compiler, and the Python packages
listed in `pyproject.toml` (`pygame`, `pytest`).

```zsh
# Build the shared library
make sim

# Run the pytest suite
make test

# Launch the pygame visual display (hold 1 = player B, 2 = player A)
make display
```

## Web simulator (Vue + WASM)

The same C++ code is compiled to WebAssembly with Emscripten and wrapped in
a Vue 3 app. Audio is synthesised via Web Audio.

```zsh
# Start Vite dev server with hot-reload
make web-dev

# Production build (output in web/dist/)
make web
```

**Prerequisites:** [Emscripten](https://emscripten.org/) (`emcc`) and Node.js.

## Wiring

**LEDs (10):** Two rows of five, directly driven from Arduino pins via
`ht16k33_display.h`:

| LED | Row | Arduino Pin |
|-----|-----|-------------|
| L1 | Top, leftmost | 6 |
| L2 | Top | 8 |
| L3 | Top | A1 |
| L4 | Top | 11 |
| L5 | Top, rightmost | 10 |
| L6 | Bottom, leftmost | 4 |
| L7 | Bottom | 7 |
| L8 | Bottom | 9 |
| L9 | Bottom | A0 |
| L10 | Bottom, rightmost | 12 |

**7-Segment Displays:** 2× 4-digit displays on HT16K33 I2C backpack at
address **0x70** (SDA = A4, SCL = A5).

**Buzzer:** Grove Passive Buzzer v1.1 on **pin 5** (PWM).

**Buttons:**
- **Pin 3** (BTN_A) — Player A (internal pull-up; connect to GND)
- **Pin 2** (BTN_B) — Player B (internal pull-up; connect to GND)
