// High-level animations for the tic-tac-toe board. There is currently
// just one RGB LED to drive, so this is a placeholder until the actual
// game logic is ported in.
#pragma once

namespace animation {

// One quick all-on flash: every color of the single RGB LED, then off.
void startupBlink();

// Idle animation tick. Call from loop() every iteration. For now this
// just slowly cycles the LED through R / G / B.
void tick();

}  // namespace animation

