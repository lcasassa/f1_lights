# Arduino Pro Mini Blink (5V / 16MHz)

This project is configured for an **Arduino Pro Mini 5V 16MHz** using PlatformIO.

## What it does

- Implements F1 start light sequence with 10 LEDs (two rows of 5)
- Press both buttons (pins 2 and 3) simultaneously to start the sequence
- LEDs light up left-to-right with random pause before GO signal
- Includes hardware test mode for debugging LED and button connections

## Project files

- `platformio.ini` - Board/framework configuration with multiple environments
- `src/main.cpp` - F1 start sequence firmware (default)
- `src/main_test.cpp` - Hardware test mode firmware
- `Makefile` - Build automation with auto-port detection
- `tools/blink_preview.py` - Local timing preview harness

## Quick start

### Using Makefile (recommended)

```zsh
# See all available targets
make help

# Run simulation
make simulate

# Compile firmware
make build

# Upload to board (auto-discovers USB port)
make upload

# Open serial monitor (auto-discovers USB port)
make monitor

# Do everything at once (simulate, build, upload)
make all

# Clean build artifacts
make clean

# Manual port override (if auto-discovery fails)
make upload PORT=/dev/cu.usbserial-XXXX
make monitor PORT=/dev/cu.usbserial-XXXX
```

### Manual PlatformIO commands

```zsh
pio run
pio run -t upload --upload-port /dev/cu.usbserial-XXXX
pio device monitor --baud 9600 --port /dev/cu.usbserial-XXXX
python3 tools/blink_preview.py
```

## Simulation & Testing

### Interactive Python Simulator
**Threaded Python simulator with real-time button/LED interaction**

```bash
make run-simulator
```

Commands: L/R (press), l/r (release), s (show state), q (quit)

### Quick Timing Preview

```bash
make simulate
```

Shows one complete sequence timing with random delays.

## Wiring

**LEDs:** Connected to pins 4-13 (configured in PIN_MAP)

**Buttons:** 
- **Pin 2** - Left button (internal pull-up)
- **Pin 3** - Right button (internal pull-up)
- Connect button ground to Arduino GND

