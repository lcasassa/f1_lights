// Low-level HT16K33 I2C driver shared by every "logical" device wired to
// the same backpack (14-seg + RGB LEDs + the two 5-digit 7-seg panels).
//
// Every writer goes through htWriteDigitShadow() so different layers that
// happen to share a COM line never clobber each other's bits.
#pragma once

#include <Arduino.h>

// Default I2C pins for the ESP32-C3 Super Mini. Override via -DI2C_SDA / -DI2C_SCL.
#ifndef I2C_SDA
#define I2C_SDA 4
#endif
#ifndef I2C_SCL
#define I2C_SCL 5
#endif

namespace ht16k33 {

// Default I2C address (A0..A2 tied low).
static constexpr uint8_t kAddr = 0x70;

// Bring the chip up: oscillator on, modest brightness, display on, RAM cleared.
void setup();

// Send a single command byte to the chip.
void cmd(uint8_t c);

// Fill all 16 display-RAM bytes with the same value.
void fillAll(uint8_t v);

// Write the full 16-bit segment word for a single digit (no shadowing).
void writeDigit(uint8_t digit, uint16_t segs);

// Update only the bits in `mask` on COM `digit`, leaving every other bit
// (owned by another logical device) untouched. Pushes the merged word.
void writeDigitShadow(uint8_t digit, uint16_t segs, uint16_t mask);

}  // namespace ht16k33

