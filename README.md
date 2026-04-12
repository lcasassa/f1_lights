# F1 Start Lights

Two-player reaction-time game built on an **Arduino Pro Mini 5V 16MHz**.
Ten LEDs (two rows of five) reproduce the Formula 1 start-light sequence; a
passive buzzer adds engine-startup and race sounds.

## How to play

1. Both players press their buttons simultaneously to start a round.
2. Five columns of LEDs light up left-to-right at 1-second intervals (with a beep for each).
3. All lights stay on for a random 1–3 second pause.
4. **Lights out — GO!** First player to press wins; their row blinks.
5. Pressing *before* lights-out is a **false start** — the other player wins automatically and a penalty LED is shown.
6. If both press at the same instant it's a **tie** and both rows blink.
7. To restart, each player must press-and-release their button (readiness expires after 2 s if the other player doesn't respond).

## Project structure

| Path | Description |
|---|---|
| `platformio.ini` | PlatformIO board/framework config (default + test environments) |
| `src/main.cpp` | F1 start-sequence game firmware |
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

# Launch the pygame visual display (hold 1 = left player, 2 = right player)
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

**LEDs (10):** Two rows of five, mapped to Arduino pins via `PIN_MAP` in
`src/main.cpp`:

| Position | Row | Arduino Pin |
|----------|-----|-------------|
| POS-1 | Top, leftmost | 4 |
| POS-2 | Top | 11 |
| POS-3 | Top | 12 |
| POS-4 | Top | A0 |
| POS-5 | Top, rightmost | A1 |
| POS-6 | Bottom, leftmost | 5 |
| POS-7 | Bottom | 10 |
| POS-8 | Bottom | 9 |
| POS-9 | Bottom | 8 |
| POS-10 | Bottom, rightmost | 7 |

**Buzzer:** Grove Passive Buzzer v1.1 on **pin 6** (PWM).

**Buttons:**
- **Pin 2** — Left player (internal pull-up; connect to GND)
- **Pin 3** — Right player (internal pull-up; connect to GND)
