#include "rgb_panel.h"

#include "ht16k33.h"

namespace rgb_panel {

namespace {

// Segment-bit positions inside the 16-bit per-digit word (verified with
// SEGMENT_SCAN on this specific backpack):
//   A=0, B=1, C=2, D=3, E=4, F=5, G1=6, G2=7, H=8, I=9, J=10, M=11,
//   L=12, DP=13 (not the textbook 14), K=14.
constexpr uint16_t SEG_A  = 1u << 0;
constexpr uint16_t SEG_B  = 1u << 1;
constexpr uint16_t SEG_C  = 1u << 2;
constexpr uint16_t SEG_D  = 1u << 3;
constexpr uint16_t SEG_E  = 1u << 4;
constexpr uint16_t SEG_F  = 1u << 5;
constexpr uint16_t SEG_G1 = 1u << 6;
constexpr uint16_t SEG_G2 = 1u << 7;
constexpr uint16_t SEG_H  = 1u << 8;
constexpr uint16_t SEG_I  = 1u << 9;
constexpr uint16_t SEG_J  = 1u << 10;
constexpr uint16_t SEG_M  = 1u << 11;
constexpr uint16_t SEG_L  = 1u << 12;
constexpr uint16_t SEG_DP = 1u << 13;
constexpr uint16_t SEG_K  = 1u << 14;

// Fill outside-in: 1 → 10 → 2 → 9 → 3 → 8 → 4 → 7 → 5 → 6.
constexpr uint8_t kOtaFillOrder[] = {0, 9, 1, 8, 2, 7, 3, 6, 4, 5};

}  // namespace

// kLeds[] quirks confirmed by running the startup chase:
//   - F/I/J pair (#3/#4): {Dig1} = #3, {Dig0} = #4 for ALL three colors.
//   - For #7..#10 the BLUE legs are crossed between pairs on the PCB:
//     red/green of the H/G2 pair live on physical #7/#8, but their blue
//     leg is wired to the C/D pair's DP segment, and vice versa.
const Led kLeds[] = {
  {0, SEG_A, SEG_G1, SEG_B},   // #1  — Dig0, A/B/G1
  {1, SEG_A, SEG_G1, SEG_B},   // #2  — Dig1, A/B/G1
  {1, SEG_J, SEG_I,  SEG_F},   // #3  — Dig1, F/I/J
  {0, SEG_J, SEG_I,  SEG_F},   // #4  — Dig0, F/I/J
  {0, SEG_E, SEG_M,  SEG_L},   // #5  — Dig0, E/M/L
  {1, SEG_E, SEG_M,  SEG_L},   // #6  — Dig1, E/M/L
  {0, SEG_H, SEG_G2, SEG_DP},  // #7  — R/G=H/G2, B=DP (crossed)
  {1, SEG_H, SEG_G2, SEG_DP},  // #8  — R/G=H/G2, B=DP (crossed)
  {1, SEG_D, SEG_C,  SEG_K},   // #9  — R/G=C/D,  B=K  (crossed)
  {0, SEG_D, SEG_C,  SEG_K},   // #10 — R/G=C/D,  B=K  (crossed)
};
const uint8_t kNumLeds = sizeof(kLeds) / sizeof(kLeds[0]);

namespace {

// Pre-compute the union of every RGB-controlled bit on each digit, used as
// the write-mask so we never disturb the seg7 bits sharing the same COM.
void rgbMaskByDigit(uint16_t out[4]) {
  for (uint8_t d = 0; d < 4; d++) out[d] = 0;
  for (uint8_t i = 0; i < kNumLeds; i++) {
    const Led &led = kLeds[i];
    out[led.digit] |= led.rMask | led.gMask | led.bMask;
  }
}

// Push per-digit RGB bits to the chip via the shared shadow.
void flush(const uint16_t bits[4], const uint16_t mask[4]) {
  for (uint8_t d = 0; d < 4; d++) {
    if (mask[d] == 0) continue;
    uint16_t segs = bits[d];
#if RGB_ACTIVE_LOW
    segs = (~segs) & mask[d];
#endif
    ht16k33::writeDigitShadow(d, segs, mask[d]);
  }
}

}  // namespace

void setAll(bool r, bool g, bool b) {
  uint16_t bits[4] = {0, 0, 0, 0};
  uint16_t mask[4]; rgbMaskByDigit(mask);
  for (uint8_t i = 0; i < kNumLeds; i++) {
    const Led &led = kLeds[i];
    if (r) bits[led.digit] |= led.rMask;
    if (g) bits[led.digit] |= led.gMask;
    if (b) bits[led.digit] |= led.bMask;
  }
  flush(bits, mask);
}

void blank() {
  setAll(false, false, false);
}

void setBusy(uint8_t ledIndex, bool on) {
  setLed(ledIndex, on, false, false);
}

void setLed(uint8_t ledIndex, bool r, bool g, bool b) {
  uint16_t bits[4] = {0, 0, 0, 0};
  uint16_t mask[4]; rgbMaskByDigit(mask);
  if (ledIndex < kNumLeds) {
    const Led &led = kLeds[ledIndex];
    if (r) bits[led.digit] |= led.rMask;
    if (g) bits[led.digit] |= led.gMask;
    if (b) bits[led.digit] |= led.bMask;
  }
  flush(bits, mask);
}

void tickRandomRed() {
  uint16_t bits[4] = {0, 0, 0, 0};
  uint16_t mask[4]; rgbMaskByDigit(mask);
  for (uint8_t i = 0; i < kNumLeds; i++) {
    const Led &led = kLeds[i];
    if (random(0, 2)) bits[led.digit] |= led.rMask;
  }
  flush(bits, mask);
}

void setRedMask(uint16_t ledMask) {
  uint16_t bits[4] = {0, 0, 0, 0};
  uint16_t mask[4]; rgbMaskByDigit(mask);
  for (uint8_t i = 0; i < kNumLeds; i++) {
    if (ledMask & (1u << i)) {
      const Led &led = kLeds[i];
      bits[led.digit] |= led.rMask;
    }
  }
  flush(bits, mask);
}

void showOtaProgress(int percent) {
  if (percent < 0)   percent = 0;
  if (percent > 100) percent = 100;
  const uint8_t filled    = (uint8_t)(percent / 10);
  const uint8_t remainder = (uint8_t)(percent % 10);
  const bool inProgressOn = (remainder & 1u) != 0;

  uint16_t bits[4] = {0, 0, 0, 0};
  uint16_t mask[4]; rgbMaskByDigit(mask);
  for (uint8_t slot = 0; slot < kNumLeds; slot++) {
    const Led &led = kLeds[kOtaFillOrder[slot]];
    bool red = (slot < filled) || (slot == filled && inProgressOn);
    if (red) bits[led.digit] |= led.rMask;
  }
  flush(bits, mask);
}

}  // namespace rgb_panel

