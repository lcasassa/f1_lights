"""
f1_sim – Python interface to the F1 Start Lights Arduino sketch.

Usage:
    from f1_sim import F1Sim

    sim = F1Sim()           # loads the shared library (builds it if needed)
    sim.setup()             # calls Arduino setup()
    sim.set_millis(0)
    sim.loop()              # calls Arduino loop()
    print(sim.led_states()) # dict mapping LED position (1-10) to bool (on/off)
    sim.press_left()        # simulate pressing left button
    sim.release_left()
"""

from __future__ import annotations

import ctypes
import platform
from pathlib import Path
from typing import Dict

_PROJECT_ROOT = Path(__file__).resolve().parent.parent

# Pin mapping — must match main.cpp
_A0 = 14
_A1 = 15
_PIN_MAP = [4, 11, 12, _A0, _A1, 5, 10, 9, 8, 7]  # POS-1..POS-10
BUTTON_LEFT_PIN = 2
BUTTON_RIGHT_PIN = 3
HIGH = 1
LOW = 0


def _lib_path() -> Path:
    ext = {"Darwin": ".dylib", "Linux": ".so", "Windows": ".dll"}.get(
        platform.system(), ".so"
    )
    return _PROJECT_ROOT / "build" / f"libf1sim{ext}"


class F1Sim:
    """High-level Python wrapper around the F1 start-lights simulation."""

    def __init__(self):
        lib = _lib_path()
        if not lib.exists():
            raise FileNotFoundError(
                f"Shared library not found at {lib}. "
                "Build it first with: make sim"
            )
        self._lib = ctypes.CDLL(str(lib))
        self._configure_api()

    # ── private helpers ──────────────────────────────────────────────────────

    def _configure_api(self):
        L = self._lib

        L.sim_setup.restype = None
        L.sim_setup.argtypes = []

        L.sim_loop.restype = None
        L.sim_loop.argtypes = []

        L.sim_set_millis.restype = None
        L.sim_set_millis.argtypes = [ctypes.c_ulong]

        L.sim_get_millis.restype = ctypes.c_ulong
        L.sim_get_millis.argtypes = []

        L.sim_advance_millis.restype = None
        L.sim_advance_millis.argtypes = [ctypes.c_ulong]

        L.sim_get_pin_value.restype = ctypes.c_uint8
        L.sim_get_pin_value.argtypes = [ctypes.c_uint8]

        L.sim_set_pin_input.restype = None
        L.sim_set_pin_input.argtypes = [ctypes.c_uint8, ctypes.c_uint8]

        L.sim_get_pin_input.restype = ctypes.c_uint8
        L.sim_get_pin_input.argtypes = [ctypes.c_uint8]

        L.sim_get_pin_mode.restype = ctypes.c_uint8
        L.sim_get_pin_mode.argtypes = [ctypes.c_uint8]

        L.sim_get_all_pin_values.restype = None
        L.sim_get_all_pin_values.argtypes = [ctypes.POINTER(ctypes.c_uint8), ctypes.c_uint8]

        L.sim_reset.restype = None
        L.sim_reset.argtypes = []

        L.sim_get_tone_freq.restype = ctypes.c_uint
        L.sim_get_tone_freq.argtypes = []

        L.sim_get_tone_pin.restype = ctypes.c_uint8
        L.sim_get_tone_pin.argtypes = []

    # ── Sketch entry-points ──────────────────────────────────────────────────

    def setup(self):
        """Call Arduino setup()."""
        self._lib.sim_setup()

    def loop(self):
        """Call Arduino loop() once."""
        self._lib.sim_loop()

    # ── Clock control ────────────────────────────────────────────────────────

    def set_millis(self, ms: int):
        self._lib.sim_set_millis(ms)

    def get_millis(self) -> int:
        return self._lib.sim_get_millis()

    def advance_millis(self, delta: int):
        self._lib.sim_advance_millis(delta)

    # ── Pin-level access ─────────────────────────────────────────────────────

    def get_pin_value(self, pin: int) -> int:
        """Get last value written by digitalWrite (HIGH/LOW)."""
        return self._lib.sim_get_pin_value(pin)

    def set_pin_input(self, pin: int, value: int):
        """Set value that digitalRead will return for *pin*."""
        self._lib.sim_set_pin_input(pin, value)

    def get_pin_input(self, pin: int) -> int:
        return self._lib.sim_get_pin_input(pin)

    # ── Tone / buzzer helpers ──────────────────────────────────────────────────

    def tone_freq(self) -> int:
        """Return the current buzzer frequency (0 = silent)."""
        return self._lib.sim_get_tone_freq()

    def tone_pin(self) -> int:
        """Return the pin the tone is currently playing on."""
        return self._lib.sim_get_tone_pin()

    # ── LED helpers ──────────────────────────────────────────────────────────

    def led_state(self, position: int) -> bool:
        """Return True if LED at *position* (1-10) is ON."""
        if position < 1 or position > 10:
            raise ValueError("position must be 1-10")
        pin = _PIN_MAP[position - 1]
        return self.get_pin_value(pin) == HIGH

    def led_states(self) -> Dict[int, bool]:
        """Return dict {1: True/False, ..., 10: True/False}."""
        return {pos: self.led_state(pos) for pos in range(1, 11)}

    def top_row(self) -> list[bool]:
        """Return list of 5 bools for top row (POS 1-5)."""
        return [self.led_state(p) for p in range(1, 6)]

    def bottom_row(self) -> list[bool]:
        """Return list of 5 bools for bottom row (POS 6-10)."""
        return [self.led_state(p) for p in range(6, 11)]

    # ── Button helpers ───────────────────────────────────────────────────────

    def press_left(self):
        """Simulate pressing the left button (active LOW with pull-up)."""
        self.set_pin_input(BUTTON_LEFT_PIN, LOW)

    def release_left(self):
        """Simulate releasing the left button."""
        self.set_pin_input(BUTTON_LEFT_PIN, HIGH)

    def press_right(self):
        """Simulate pressing the right button."""
        self.set_pin_input(BUTTON_RIGHT_PIN, LOW)

    def release_right(self):
        """Simulate releasing the right button."""
        self.set_pin_input(BUTTON_RIGHT_PIN, HIGH)

    def press_both(self):
        self.press_left()
        self.press_right()

    def release_both(self):
        self.release_left()
        self.release_right()

    # ── Full reset ───────────────────────────────────────────────────────────

    def reset(self):
        """Reset all simulated pin state and clock to zero.

        NOTE: This only resets the Arduino stub state (pins, clock).
        The sketch's own global variables (currentState, etc.) are NOT
        reset because they live in the same process.  Call setup() after
        reset() to re-initialise the sketch.
        """
        self._lib.sim_reset()

    # ── Convenience: tick ─────────────────────────────────────────────────────

    def tick(self, delta_ms: int = 1, loops: int = 1):
        """Advance the clock by *delta_ms* and call loop() *loops* times."""
        self.advance_millis(delta_ms)
        for _ in range(loops):
            self.loop()

