#include "rgb_panel.h"

#include "ht16k33.h"

namespace rgb_panel {

namespace {

// Segment-bit positions inside the 16-bit per-digit word (verified on
// the F1 backpack with SEGMENT_SCAN — same chip, same wiring rules):
//   A=0, B=1, C=2, D=3, E=4, F=5, G1=6, G2=7, H=8, I=9, J=10, M=11,
//   L=12, DP=13, K=14.
constexpr uint16_t SEG_D  = 1u << 3;
constexpr uint16_t SEG_G2 = 1u << 7;
constexpr uint16_t SEG_DP = 1u << 13;

constexpr uint8_t  kDigit = TICTACTOE_RGB_DIGIT;
constexpr uint16_t kRMask = SEG_G2;
constexpr uint16_t kGMask = SEG_D;
constexpr uint16_t kBMask = SEG_DP;
constexpr uint16_t kMask  = kRMask | kGMask | kBMask;

void writeRgb(bool r, bool g, bool b) {
  uint16_t segs = 0;
  if (r) segs |= kRMask;
  if (g) segs |= kGMask;
  if (b) segs |= kBMask;
#if RGB_ACTIVE_LOW
  segs = (~segs) & kMask;
#endif
  ht16k33::writeDigitShadow(kDigit, segs, kMask);
}

}  // namespace

const uint8_t kNumLeds = 1;

void setAll(bool r, bool g, bool b)               { writeRgb(r, g, b); }
void blank()                                      { writeRgb(false, false, false); }
void setBusy(uint8_t /*ledIndex*/, bool on)       { writeRgb(on, false, false); }
void setLed(uint8_t /*ledIndex*/, bool r, bool g, bool b) { writeRgb(r, g, b); }

void showOtaProgress(int percent) {
  if (percent <= 0)        writeRgb(false, false, false);
  else if (percent >= 100) writeRgb(false, true,  false);   // done = green
  else                     writeRgb(true,  false, false);   // in-flight = red
}

}  // namespace rgb_panel

