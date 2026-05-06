#include "ht16k33.h"

#include <Wire.h>

namespace ht16k33 {

namespace {
constexpr uint8_t kSysOn       = 0x21;  // oscillator on
constexpr uint8_t kDisplayOn   = 0x81;  // display on, no blink
constexpr uint8_t kDimmingBase = 0xE0;  // | brightness 0..15

// Per-digit shadow shared by every writer.
uint16_t g_shadow[8] = {0};
}  // namespace

void cmd(uint8_t c) {
  Wire.beginTransmission(kAddr);
  Wire.write(c);
  Wire.endTransmission();
}

void fillAll(uint8_t v) {
  Wire.beginTransmission(kAddr);
  Wire.write(0x00);
  for (uint8_t i = 0; i < 16; i++) Wire.write(v);
  Wire.endTransmission();
  for (uint8_t d = 0; d < 8; d++) {
    g_shadow[d] = (uint16_t)v | ((uint16_t)v << 8);
  }
}

void writeDigit(uint8_t digit, uint16_t segs) {
  Wire.beginTransmission(kAddr);
  Wire.write((uint8_t)(digit * 2));
  Wire.write((uint8_t)(segs & 0xFF));
  Wire.write((uint8_t)((segs >> 8) & 0xFF));
  Wire.endTransmission();
}

void writeDigitShadow(uint8_t digit, uint16_t segs, uint16_t mask) {
  g_shadow[digit] = (g_shadow[digit] & ~mask) | (segs & mask);
  writeDigit(digit, g_shadow[digit]);
}

void setup() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000);  // HT16K33 supports up to 400 kHz
  cmd(kSysOn);
  cmd(kDimmingBase | 2);
  cmd(kDisplayOn);
  fillAll(0x00);
  Serial.println("HT16K33: setup done");
}

}  // namespace ht16k33


