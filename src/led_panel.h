#pragma once
#include <Arduino.h>

// ── Red LED Panel (L1-L10) — direct Arduino pins ─────────────────────────
//
// 10 red LEDs driven directly from Arduino digital/analog pins.
// Extracted from HT16K33Display so the I2C display driver stays focused
// on the 7-segment digits only.

class LedPanel {
public:
  static constexpr uint8_t NUM_LEDS = 10;

  void begin() {
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      pinMode(pins_[i], OUTPUT);
      digitalWrite(pins_[i], LOW);
    }
  }

  void setLed(uint8_t led, bool on) {  // led = 1..10
    if (led < 1 || led > NUM_LEDS) return;
    digitalWrite(pins_[led - 1], on ? HIGH : LOW);
  }

  void setAllLeds(bool on) {
    for (uint8_t i = 1; i <= NUM_LEDS; i++) setLed(i, on);
  }

private:
  static constexpr uint8_t pins_[10] = {
    6,   // L1
    8,   // L2
    A1,  // L3
    11,  // L4
    10,  // L5
    4,   // L6
    7,   // L7
    9,   // L8
    A0,  // L9
    12,  // L10
  };
};

constexpr uint8_t LedPanel::pins_[10];

