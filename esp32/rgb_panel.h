// 10 RGB LEDs piggy-backed on the 14-seg HT16K33 backpack. Each color leg
// of each LED is driven by one HT16K33 segment line on a given digit; see
// kRgbLeds[] in the .cpp for the live wiring map.
//
// Set RGB_ACTIVE_LOW=1 in the build env if your LEDs are common-anode
// (segment line LOW = color ON). Default is common-cathode (HIGH = ON).
#pragma once

#include <Arduino.h>

#ifndef RGB_ACTIVE_LOW
#define RGB_ACTIVE_LOW 0
#endif

namespace rgb_panel {

struct Led {
  uint8_t  digit;
  uint16_t rMask;
  uint16_t gMask;
  uint16_t bMask;
};

// Number of physical LEDs on the panel.
extern const uint8_t kNumLeds;

// kLeds[i] is the LED silkscreened "#i+1".
extern const Led kLeds[];

// Drive every LED to the same boolean R/G/B mix. The HT16K33 has only a
// global brightness register, so per-color PWM isn't possible.
void setAll(bool r, bool g, bool b);

// Force every RGB-controlled bit to "off" (respects RGB_ACTIVE_LOW).
void blank();

// "Busy" indicator: light a single LED (0-based index into kLeds) red and
// blank the others. Used during long blocking calls (WiFi associate,
// HTTPS GET, …) where there's no real progress percentage to show yet.
// Pass `on = false` to clear.
void setBusy(uint8_t ledIndex, bool on);

// Light a single LED (0-based index) to an arbitrary R/G/B mix and blank
// every other RGB-controlled bit. Useful for one-off status indicators
// (e.g. yellow = lean build, blue = AP mode) without having to reason
// about per-digit segment masks.
void setLed(uint8_t ledIndex, bool r, bool g, bool b);

// Light an arbitrary subset of LEDs red (bit `i` in `mask` = LED `i` on)
// in a single I²C transaction. All other RGB-controlled bits blanked.
// Useful for sequencer animations (F1 starting lights, …).
void setRedMask(uint16_t mask);

// One frame of the random per-LED red flicker (50/50 per LED). Call from
// a paced loop — does no timing of its own.
void tickRandomRed();

// Render `percent` (clamped to 0..100) as a fill-from-the-edges red bar.
// Each LED = 10 percentage points; the in-progress LED blinks every 1%
// as a heartbeat between coarse 10% steps.
void showOtaProgress(int percent);

}  // namespace rgb_panel

