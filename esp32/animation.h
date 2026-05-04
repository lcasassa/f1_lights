// High-level "device" animations: the boot self-test flash and the
// button-driven flicker + counter sequence used in the main loop.
#pragma once

namespace animation {

// One quick all-on flash: every RGB LED red and both 5-digit 7-seg panels
// showing "8.8.8.8.8.", then everything blanked.
void startupBlink();

// Drive the button-gated random-flicker + auto-incrementing counter. Call
// from loop() at every iteration; tracks its own internal pacing and idle
// state. `running` should be true while at least one button is held.
void tick(bool running);

}  // namespace animation

