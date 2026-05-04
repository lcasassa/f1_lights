#include "seg7.h"

#include <string.h>

#include "ht16k33.h"

namespace seg7 {

const Map kTop = {
  /*A */ 1u << 5,  /*B */ 1u << 9,  /*C */ 1u << 0,  /*D */ 1u << 10,
  /*E */ 1u << 1,  /*F */ 1u << 8,  /*G */ 1u << 6,  /*DP*/ 1u << 2,
};

const Map kBot = {
  /*A */ 1u << 3,  /*B */ 1u << 15, /*C */ 1u << 11, /*D */ 1u << 13,
  /*E */ 1u << 12, /*F */ 1u << 14, /*G */ 1u << 4,  /*DP*/ 1u << 7,
};

namespace {

// Both displays share the same 5 character-cell → COM mapping.
//   cell 0 (far left)  → COM 7
//   cell 1             → COM 6
//   cell 2             → COM 5
//   cell 3             → COM 3   (note: COM 4 is skipped on this wiring)
//   cell 4 (far right) → COM 2
constexpr uint8_t kComs[] = {7, 6, 5, 3, 2};
constexpr uint8_t kNumDigits = sizeof(kComs) / sizeof(kComs[0]);

// Standard 7-seg glyphs, parameterised by which Map we're rendering for.
uint16_t encode(const Map &m, char c) {
  switch (c) {
    case '0': return m.A | m.B | m.C | m.D | m.E | m.F;
    case '1': return m.B | m.C;
    case '2': return m.A | m.B | m.G | m.E | m.D;
    case '3': return m.A | m.B | m.G | m.C | m.D;
    case '4': return m.F | m.G | m.B | m.C;
    case '5': return m.A | m.F | m.G | m.C | m.D;
    case '6': return m.A | m.F | m.G | m.E | m.C | m.D;
    case '7': return m.A | m.B | m.C;
    case '8': return m.A | m.B | m.C | m.D | m.E | m.F | m.G;
    case '9': return m.A | m.B | m.C | m.D | m.F | m.G;
    case '-': return m.G;
    case '_': return m.D;
    case ' ': return 0;
    case 'A': case 'a': return m.A | m.B | m.C | m.E | m.F | m.G;
    case 'b':           return m.C | m.D | m.E | m.F | m.G;
    case 'C': case 'c': return m.A | m.D | m.E | m.F;
    case 'd':           return m.B | m.C | m.D | m.E | m.G;
    case 'E': case 'e': return m.A | m.D | m.E | m.F | m.G;
    case 'F': case 'f': return m.A | m.E | m.F | m.G;
    case 'H': case 'h': return m.B | m.C | m.E | m.F | m.G;
    case 'L': case 'l': return m.D | m.E | m.F;
    case 'P': case 'p': return m.A | m.B | m.E | m.F | m.G;
    case 'r':           return m.E | m.G;
    case 'o':           return m.C | m.D | m.E | m.G;
    case 'S': case 's': return m.A | m.F | m.G | m.C | m.D;
    case 'U': case 'u': case 'V': case 'v':
                        return m.B | m.C | m.D | m.E | m.F;   // big "U"
    default:            return 0;
  }
}

}  // namespace

void writeText(const Map &m, const char *s) {
  uint16_t glyphs[kNumDigits];
  bool     dps[kNumDigits];
  for (uint8_t i = 0; i < kNumDigits; i++) { glyphs[i] = 0; dps[i] = false; }

  size_t len = strlen(s);
  int8_t out = (int8_t)kNumDigits - 1;
  for (int i = (int)len - 1; i >= 0 && out >= 0; i--) {
    char c = s[i];
    if (c == '.') {
      if (i == 0) break;
      int j = i - 1;
      while (j >= 0 && s[j] == '.') j--;
      if (j < 0) break;
      glyphs[out] = encode(m, s[j]);
      dps[out]    = true;
      out--;
      i = j;
    } else {
      glyphs[out] = encode(m, c);
      out--;
    }
  }

  const uint16_t clearMask = m.mask();
  for (uint8_t i = 0; i < kNumDigits; i++) {
    uint16_t segs = glyphs[i] | (dps[i] ? m.DP : 0);
    ht16k33::writeDigitShadow(kComs[i], segs, clearMask);
  }
}

void printFloat(const Map &m, float value) {
  bool neg = value < 0;
  if (neg) value = -value;

  uint32_t intPart = (uint32_t)value;
  uint8_t  intDigits = 1;
  for (uint32_t t = intPart; t >= 10; t /= 10) intDigits++;

  uint8_t budget = kNumDigits - (neg ? 1 : 0);
  if (intDigits > budget) {
    char buf[kNumDigits + 1];
    for (uint8_t i = 0; i < kNumDigits; i++) buf[i] = '-';
    buf[kNumDigits] = '\0';
    writeText(m, buf);
    return;
  }

  uint8_t fracDigits = budget - intDigits;
  float scale = 1.0f;
  for (uint8_t i = 0; i < fracDigits; i++) scale *= 10.0f;
  uint32_t scaled = (uint32_t)(value * scale + 0.5f);

  uint32_t newIntPart  = scaled / (uint32_t)scale;
  uint32_t newFracPart = scaled % (uint32_t)scale;
  uint8_t  newIntDigits = 1;
  for (uint32_t t = newIntPart; t >= 10; t /= 10) newIntDigits++;
  if (newIntDigits > budget) {
    char buf[kNumDigits + 1];
    for (uint8_t i = 0; i < kNumDigits; i++) buf[i] = '-';
    buf[kNumDigits] = '\0';
    writeText(m, buf);
    return;
  }

  char buf[16];
  int  pos = 0;
  if (neg) buf[pos++] = '-';
  char tmp[12];
  int  tlen = 0;
  if (newIntPart == 0) tmp[tlen++] = '0';
  while (newIntPart > 0) { tmp[tlen++] = (char)('0' + newIntPart % 10); newIntPart /= 10; }
  while (tlen > 0) buf[pos++] = tmp[--tlen];
  if (fracDigits > 0) {
    buf[pos++] = '.';
    char ftmp[12];
    int  flen = 0;
    for (uint8_t i = 0; i < fracDigits; i++) {
      ftmp[flen++] = (char)('0' + newFracPart % 10);
      newFracPart /= 10;
    }
    while (flen > 0) buf[pos++] = ftmp[--flen];
  }
  buf[pos] = '\0';
  writeText(m, buf);
}

void clear(const Map &m) {
  const uint16_t clearMask = m.mask();
  for (uint8_t i = 0; i < kNumDigits; i++) {
    ht16k33::writeDigitShadow(kComs[i], 0, clearMask);
  }
}

}  // namespace seg7

