// High-level "device" animations: the boot self-test flash and the
// button-driven flicker + counter sequence used in the main loop.
#pragma once

namespace animation {

// One quick all-on flash: every RGB LED red and both 5-digit 7-seg panels
// showing "8.8.8.8.8.", then everything blanked.
void startupBlink();

// F1 starting-light state machine. Call from loop() every iteration with
// the live "any button pressed" state. The first rising edge starts the
// 5-column lights-on sequence; the lights then extinguish after a random
// hold and the next press is timed in milliseconds and shown on the top
// 7-seg display. A press during the hold is treated as a jump start.
void tick(bool pressed);

}  // namespace animation

