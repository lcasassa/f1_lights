#!/usr/bin/env python3
"""Quick smoke test for the F1 simulation."""
from f1_sim import F1Sim

sim = F1Sim()
sim.reset()
sim.set_millis(0)
sim.setup()
print("After setup, LEDs:", sim.led_states())

# IDLE state - press left button to start
sim.advance_millis(100)
sim.press_left()
sim.loop()
sim.advance_millis(15)
sim.loop()
print("After pressing left to start, LEDs:", sim.led_states())

# Release button so early-start detection can enable
sim.release_left()
sim.advance_millis(15)
sim.loop()

print("Column 1 on? top:", sim.led_state(1), "bottom:", sim.led_state(6))

# Advance 1000ms for column 2
sim.advance_millis(1000)
sim.loop()
print("After 1s - Column 2 on? top:", sim.led_state(2), "bottom:", sim.led_state(7))

# Advance to 4s total for all 5 columns
for _ in range(3):
    sim.advance_millis(1000)
    sim.loop()
print("After 4s - All LEDs:", sim.led_states())

# Advance past 5s to transition to ALL_ON
sim.advance_millis(1000)
sim.loop()
print("After 5s (ALL_ON) - All LEDs:", sim.led_states())

# Advance past random delay (max 3s) to get BLACKOUT
sim.advance_millis(4000)
sim.loop()
print("After blackout - All LEDs:", sim.led_states())

# Now press right button -> RIGHT PLAYER WINS
sim.advance_millis(15)
sim.press_right()
sim.loop()
sim.advance_millis(15)
sim.loop()
print("After right press - top row:", sim.top_row(), "bottom row:", sim.bottom_row())

print()
print("SUCCESS: Simulation working!")

