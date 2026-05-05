// High-level "device" animations: the boot self-test flash and the
// button-driven flicker + counter sequence used in the main loop.
#pragma once

namespace animation {

// One quick all-on flash: every RGB LED red and both 5-digit 7-seg panels
// showing "8.8.8.8.8.", then everything blanked.
void startupBlink();

// F1 starting-light state machine. Call from loop() every iteration with
// the live state of both front-panel buttons. The first rising edge on
// either starts the 5-column lights-on sequence; after the random hold,
// each player's reaction time is recorded independently — A → top
// 7-seg, B → bottom 7-seg — and stays on display until the next
// sequence is triggered. A press during the hold is treated as a jump
// start.
void tick(bool aPressed, bool bPressed);

}  // namespace animation

