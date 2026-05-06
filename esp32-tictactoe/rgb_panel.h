// Single RGB LED driven through the HT16K33 14-seg backpack.
//
// The tic-tac-toe board has exactly one RGB LED, soldered onto the
// 14-seg digit at COM index 4 with:
//   R leg -> segment G2
//   G leg -> segment D
//   B leg -> segment DP
//
// The API mirrors the F1 build's rgb_panel so wifi_ota.cpp compiles
// unchanged: indices are accepted but ignored (there is only one LED),
// and showOtaProgress() collapses to a simple "in-progress red /
// finished green" indicator.
//
// Set RGB_ACTIVE_LOW=1 in the build env if the LED is common-anode
// (segment line LOW = color ON). Default is common-cathode (HIGH = ON).
#pragma once

#include <Arduino.h>

#ifndef RGB_ACTIVE_LOW
#define RGB_ACTIVE_LOW 0
#endif

#ifndef TICTACTOE_RGB_DIGIT
#define TICTACTOE_RGB_DIGIT 4
#endif

namespace rgb_panel {

// Constant set to 1 so call sites that loop over LEDs still work.
extern const uint8_t kNumLeds;

// Drive the LED to the given R/G/B mix.
void setAll(bool r, bool g, bool b);

// Force the LED off (respects RGB_ACTIVE_LOW).
void blank();

// "Busy" indicator: ledIndex is ignored. `on` -> red, off -> blank.
void setBusy(uint8_t ledIndex, bool on);

// Light the LED to an arbitrary R/G/B mix. ledIndex is ignored.
void setLed(uint8_t ledIndex, bool r, bool g, bool b);

// OTA progress: 0 -> blank, 1..99 -> red, 100 -> green. Called per
// progress tick; the simple mapping gives an obvious "running /
// finished" signal on a single LED.
void showOtaProgress(int percent);

}  // namespace rgb_panel

