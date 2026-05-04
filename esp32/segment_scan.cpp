#include "segment_scan.h"

#if defined(SEGMENT_SCAN) && (SEGMENT_SCAN)

#include <Arduino.h>

#include "ht16k33.h"

#ifndef SEGMENT_SCAN_MODE
#define SEGMENT_SCAN_MODE 1   // 0 = sequential, 1 = interactive binary search
#endif

namespace segment_scan {
namespace {

// HT16K33 has 8 COM lines → 8 addressable digits of 16 bits each.
constexpr uint8_t kNumDigits = 8;
constexpr uint8_t kDigitMask = 0xFF;

void writeAllDigits(uint16_t segs) {
  for (uint8_t d = 0; d < kNumDigits; d++) ht16k33::writeDigit(d, segs);
}

[[noreturn]] void runSequential() {
  Serial.println("SEGMENT_SCAN(seq): walking all 8 digits, one bit at a time");
  while (true) {
    for (uint8_t bit = 0; bit < 16; bit++) {
      uint16_t mask = (uint16_t)(1u << bit);
      ht16k33::fillAll(0x00);
      writeAllDigits(mask);
      Serial.printf("  bit %2u → 0x%04X\n", bit, mask);
      delay(700);
    }
  }
}

char readSerialChar() {
  for (;;) {
    while (!Serial.available()) delay(5);
    int c = Serial.read();
    if (c < 0) continue;
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
    char ch = (char)c;
    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
    Serial.printf(" -> '%c'\n", ch);
    return ch;
  }
}

void printBits(uint16_t mask) {
  bool first = true;
  for (uint8_t b = 0; b < 16; b++) {
    if (mask & (1u << b)) {
      Serial.printf("%s%u", first ? "" : ",", b);
      first = false;
    }
  }
}

uint16_t lowerHalfOf(uint16_t candidates) {
  uint8_t total = __builtin_popcount(candidates);
  uint8_t want  = total / 2; if (want == 0) want = 1;
  uint16_t out = 0;
  uint8_t taken = 0;
  for (uint8_t b = 0; b < 16 && taken < want; b++) {
    uint16_t m = (uint16_t)(1u << b);
    if (candidates & m) { out |= m; taken++; }
  }
  return out;
}

[[noreturn]] void runBinary() {
  Serial.println(
    "\nSEGMENT_SCAN(binary): two-stage interactive bit-finder\n"
    "  Search universe = all 8 HT16K33 COM lines (digits 0..7)\n"
    "\n"
    "  Stage 1: narrow down which DIGIT your target lives on by lighting\n"
    "           ALL 16 segments on half of the candidate digits.\n"
    "  Stage 2: narrow down which BIT inside that digit.\n"
    "  Answers:\n"
    "    y = my target IS lit in this set\n"
    "    n = my target is NOT lit\n"
    "    a = light EVERYTHING on every candidate digit for 1 s\n"
    "    r = restart from stage 1\n"
    "    q = quit (returns to normal firmware)\n"
    "  ~7 questions total to find one bit on one digit out of 8 digits.\n");

  while (true) {
    uint8_t  digCands = kDigitMask;
    uint8_t  question = 1;
    uint16_t allBits  = 0xFFFF;

    while (__builtin_popcount(digCands) > 1) {
      uint8_t total = __builtin_popcount(digCands);
      uint8_t want  = total / 2; if (want == 0) want = 1;
      uint8_t probeDigits = 0, taken = 0;
      for (uint8_t d = 0; d < kNumDigits && taken < want; d++) {
        if (digCands & (1u << d)) { probeDigits |= (1u << d); taken++; }
      }

      ht16k33::fillAll(0x00);
      for (uint8_t d = 0; d < kNumDigits; d++) {
        if (probeDigits & (1u << d)) ht16k33::writeDigit(d, allBits);
      }

      Serial.printf("\nQ%u (digit)  candidates [", question++);
      for (uint8_t d = 0, first = 1; d < kNumDigits; d++) {
        if (digCands & (1u << d)) { Serial.printf("%s%u", first ? "" : ",", d); first = 0; }
      }
      Serial.printf("]  lighting [");
      for (uint8_t d = 0, first = 1; d < kNumDigits; d++) {
        if (probeDigits & (1u << d)) { Serial.printf("%s%u", first ? "" : ",", d); first = 0; }
      }
      Serial.printf("]  → is your target lit? (y/n/a/r/q)");
      char ans = readSerialChar();
      if (ans == 'r') { digCands = kDigitMask; question = 1; continue; }
      if (ans == 'q') { ht16k33::fillAll(0x00); Serial.println("quit"); delay(200); ESP.restart(); }
      if (ans == 'a') {
        Serial.println("  ALL ON 1 s ...");
        for (uint8_t d = 0; d < kNumDigits; d++) ht16k33::writeDigit(d, 0xFFFF);
        delay(1000);
        question--;
        continue;
      }
      if      (ans == 'y') digCands &=  probeDigits;
      else if (ans == 'n') digCands &= ~probeDigits;
      else { Serial.println("  ?? expected y/n/a/r/q"); question--; }
    }

    if (digCands == 0) { Serial.println("\nSEGMENT_SCAN: no digit candidate left. Restart."); continue; }

    uint8_t digit = (uint8_t)__builtin_ctz(digCands);
    Serial.printf("\n→ digit narrowed to %u, now searching its 16 bits\n", digit);

    uint16_t cands = 0xFFFF;
    while (__builtin_popcount(cands) > 1) {
      uint16_t probe = lowerHalfOf(cands);
      ht16k33::fillAll(0x00);
      ht16k33::writeDigit(digit, probe);
      Serial.printf("\nQ%u (bit)  candidates [", question++);
      printBits(cands);
      Serial.printf("]  lighting [");
      printBits(probe);
      Serial.printf("]  → is your target lit? (y/n/a/r/q)");
      char ans = readSerialChar();
      if (ans == 'r') { cands = 0xFFFF; continue; }
      if (ans == 'q') { ht16k33::fillAll(0x00); Serial.println("quit"); delay(200); ESP.restart(); }
      if (ans == 'a') {
        Serial.println("  ALL ON 1 s ...");
        ht16k33::writeDigit(digit, 0xFFFF);
        delay(1000);
        ht16k33::writeDigit(digit, probe);
        question--;
        continue;
      }
      if      (ans == 'y') cands &=  probe;
      else if (ans == 'n') cands &= ~probe;
      else { Serial.println("  ?? expected y/n/a/r/q"); question--; }
    }

    if (cands == 0) { Serial.println("\nSEGMENT_SCAN: no bit candidate left. Restart."); continue; }

    uint8_t bit = (uint8_t)__builtin_ctz(cands);
    ht16k33::fillAll(0x00);
    ht16k33::writeDigit(digit, cands);
    Serial.printf("\n*** FOUND: digit %u, bit %u  (mask 0x%04X) ***\n", digit, bit, cands);
    Serial.println("    Update the matching SEG_* / kLeds[] in rgb_panel.cpp.");
    Serial.println("    Press 'r' to find another segment, 'q' to quit.");
    char ans = readSerialChar();
    if (ans == 'q') { ht16k33::fillAll(0x00); Serial.println("quit"); delay(200); ESP.restart(); }
  }
}

}  // namespace

[[noreturn]] void run() {
#if SEGMENT_SCAN_MODE == 0
  runSequential();
#else
  runBinary();
#endif
}

}  // namespace segment_scan

#endif  // SEGMENT_SCAN

