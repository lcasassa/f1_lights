// 16×32 WS2812 matrix made of two 8×32 vertical-zig-zag panels.
//
// Panel layout (matches screen/main.cpp on the AVR build):
//   Panel A on PIN_A: columns 0..7   (even rows R→L, odd rows L→R)
//   Panel B on PIN_B: columns 8..15  (mirrored X + flipped Y)
//
// Coordinates: (0,0) = top-left, x grows right, y grows down.
// Pixel format here is 8-bit packed   [RR GG BB xx]   with 2-bit
// channels (same as the AVR driver), expanded through a 4-entry LUT
// when pushed to the strip. Lets the game logic stay byte-per-pixel
// while the actual NeoPixel buffer is full 24-bit.
#pragma once
#include <Arduino.h>

namespace screen_panel {

constexpr uint16_t PANEL_W   = 8;
constexpr uint16_t PANEL_H   = 32;
constexpr uint16_t MATRIX_W  = 16;
constexpr uint16_t MATRIX_H  = 32;
constexpr uint16_t LEDS_PER_PANEL = PANEL_W * PANEL_H;

// Default WS2812 data pins (override with -DPANEL_A_PIN=… / -DPANEL_B_PIN=…)
#ifndef PANEL_A_PIN
#define PANEL_A_PIN 6
#endif
#ifndef PANEL_B_PIN
#define PANEL_B_PIN 7
#endif

void setup();

// Set / clear / push the back-buffer.
void clearAll();
void setPixel(uint8_t x, uint8_t y, uint8_t packed);
void show();

// Helpers to build the 8-bit packed pixel from 2-bit channels.
constexpr uint8_t pack(uint8_t r2, uint8_t g2, uint8_t b2) {
  return (uint8_t)(((r2 & 3) << 6) | ((g2 & 3) << 4) | ((b2 & 3) << 2));
}

}  // namespace screen_panel

