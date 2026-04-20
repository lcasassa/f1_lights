#pragma once
#include <Arduino.h>
#include <Wire.h>

// ── HT16K33 7-Segment + LED Driver ────────────────────────────────────────
//
// Hardware: 2× 12-pin 4-digit 7-segment displays wired to an Adafruit
//           HT16K33 Quad Alphanumeric backpack (0x70).
//           10 red LEDs (L1-L10) on direct Arduino pins.
//
// The HT16K33 has 16 ROW × 8 COM = 128 LED positions.
// The backpack routes ROW0-ROW14 (15 lines) and COM0-COM3 (4 lines).
// Both displays share COM0-COM3 → 15 ROW × 4 COM = 60 positions.
//
// ── Rewire applied ──────────────────────────────────────────────────────
// Display A (digits 1-4): E was on G2 pad (ROW7), coupled with Display B's G.
// Fix: moved Display A's E wire from ROW7 to ROW0 (formerly DP).
//      Display A's DP is now disconnected (UNAVAIL).
//      Display B's G on ROW7 is fully independent.
//
// Remaining issues:
//   {6,4} = 8D also ghosts 4B (hardware coupled on COM3)
//   8A: rewired to ROW2 pad → {6,2} (follows Display B pattern)

class HT16K33Display {
public:
  static constexpr uint8_t I2C_ADDR = 0x70;

  // Digit indices (1-based labels → 0-based array index)
  // Group A = digits 1-4, Group B = digits 5-8
  static constexpr uint8_t NUM_DIGITS = 8;
  static constexpr uint8_t NUM_LEDS   = 10;

  void begin() {
    Wire.begin();
    cmd(0x21);        // oscillator on
    cmd(0x81);        // display on, no blink
    cmd(0xE0 | 15);   // max brightness
    clear();
    // Initialise direct-drive LED pins
    for (uint8_t i = 0; i < NUM_LEDS; i++) {
      pinMode(ledPins_[i], OUTPUT);
      digitalWrite(ledPins_[i], LOW);
    }
  }

  void clear() {
    memset(buf_, 0, sizeof(buf_));
    write();
  }

  // brightness: 0-15
  void setBrightness(uint8_t b) {
    cmd(0xE0 | (b & 0x0F));
  }

  // ── Number display ──────────────────────────────────────────────────────
  // Display a number (0-9999) on digits 1-4 (group A) or digits 5-8 (group B).
  // Leading zeros are blanked unless showLeadingZeros is true.

  void setNumberA(int16_t num, bool showLeadingZeros = false) {
    setNumber(num, 0, showLeadingZeros);  // digits 0-3 (labels 1-4)
  }

  void setNumberB(int16_t num, bool showLeadingZeros = false) {
    setNumber(num, 4, showLeadingZeros);  // digits 4-7 (labels 5-8)
  }

  // Display a single digit value (0-9) or blank (val < 0) at position 1-8.
  void setDigit(uint8_t pos, int8_t val) {
    if (pos < 1 || pos > 8) return;
    uint8_t idx = pos - 1;
    if (val < 0 || val > 9) {
      setSegments(idx, 0x00);
    } else {
      setSegments(idx, FONT[val]);
    }
  }

  // Set decimal point on digit 1-8
  void setDP(uint8_t pos, bool on) {
    if (pos < 1 || pos > 8) return;
    setSegment(pos - 1, 7, on);  // DP is segment index 7
  }

  // Show dash (segment G only) on digits 1-4 or 5-8
  void setDashA() { for (uint8_t i = 0; i < 4; i++) setSegments(i, SEG_G); }
  void setDashB() { for (uint8_t i = 4; i < 8; i++) setSegments(i, SEG_G); }

  // Set raw segment bitmask on digit 1-8
  // Bits: 0=A, 1=B, 2=C, 3=D, 4=E, 5=F, 6=G, 7=DP
  void setRaw(uint8_t pos, uint8_t segs) {
    if (pos < 1 || pos > 8) return;
    setSegments(pos - 1, segs);
  }

  // Set raw segment bitmask on digits 1-4 or 5-8
  void setRawA(uint8_t segs) { for (uint8_t i = 0; i < 4; i++) setSegments(i, segs); }
  void setRawB(uint8_t segs) { for (uint8_t i = 4; i < 8; i++) setSegments(i, segs); }

  // Query segment mapping: digit 0-7, seg 0-7 → byteIdx, bitIdx (0xFF = unavail)
  void getSegMap(uint8_t digit, uint8_t seg, uint8_t &byteIdx, uint8_t &bitIdx) const {
    byteIdx = segMap_[digit][seg][0];
    bitIdx  = segMap_[digit][seg][1];
  }

  // Segment bit constants
  static constexpr uint8_t A  = 0x01;
  static constexpr uint8_t B  = 0x02;
  static constexpr uint8_t C  = 0x04;
  static constexpr uint8_t D  = 0x08;
  static constexpr uint8_t E  = 0x10;
  static constexpr uint8_t F  = 0x20;
  static constexpr uint8_t G  = 0x40;
  static constexpr uint8_t DP = 0x80;

  //  Standard 7-segment font:
  //     _A_
  //   |F   |B
  //     _G_
  //   |E   |C
  //     _D_   .DP
  //
  static constexpr uint8_t FONT[10] = {
    A | B | C | D | E | F,         // 0
    B | C,                          // 1
    A | B | D | E | G,              // 2
    A | B | C | D | G,              // 3
    B | C | F | G,                  // 4
    A | C | D | F | G,              // 5
    A | C | D | E | F | G,          // 6
    A | B | C,                      // 7
    A | B | C | D | E | F | G,      // 8
    A | B | C | D | F | G,          // 9
  };

  // ── Red LEDs (L1-L10) — direct Arduino pins ───────────────────────────

  void setLed(uint8_t led, bool on) {  // led = 1..10
    if (led < 1 || led > NUM_LEDS) return;
    digitalWrite(ledPins_[led - 1], on ? HIGH : LOW);
  }

  void setAllLeds(bool on) {
    for (uint8_t i = 1; i <= NUM_LEDS; i++) setLed(i, on);
  }

  // ── Flush buffer to hardware ───────────────────────────────────────────

  void write() {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(0x00);
    for (uint8_t i = 0; i < 16; i++) Wire.write(buf_[i]);
    Wire.endTransmission();
  }

private:
  uint8_t buf_[16] = {};

  // ── Direct-drive LED pin mapping (L1..L10) ─────────────────────────────
  static constexpr uint8_t ledPins_[10] = {

    6,  // L1
    8,  // L2
    A1, // L3
    11, // L4
    10, // L5
    4,  // L6
    7,  // L7
    9,  // L8
    A0, // L9
    12, // L10

  };

  // Segment bit flags (private aliases)
  static constexpr uint8_t SEG_A  = A;
  static constexpr uint8_t SEG_B  = B;
  static constexpr uint8_t SEG_C  = C;
  static constexpr uint8_t SEG_D  = D;
  static constexpr uint8_t SEG_E  = E;
  static constexpr uint8_t SEG_F  = F;
  static constexpr uint8_t SEG_G  = G;
  static constexpr uint8_t SEG_DP = DP;

  // ── Per-digit segment → (byte, bit) mapping ────────────────────────────
  // Indexed as segMap_[digit_0based][segment] = {byte, bit}
  // segment order: A=0, B=1, C=2, D=3, E=4, F=5, G=6, DP=7
  // {0xFF, 0} means segment is physically unavailable.

  static constexpr uint8_t UNAVAIL = 0xFF;

  static constexpr uint8_t segMap_[8][8][2] = {
    // Digit 1 (label "1")
    {
      {0, 5},         // A
      {1, 1},         // B
      {0, 6},         // C
      {0, 1},         // D
      {0, 0},         // E  — rewired to ROW0 pad (formerly DP)
      {1, 0},         // F
      {1, 2},         // G
      {UNAVAIL, 0},   // DP — sacrificed, ROW0 now used for E
    },
    // Digit 2 (label "2")
    {
      {2, 5},         // A
      {3, 1},         // B
      {2, 6},         // C
      {2, 1},         // D
      {2, 0},         // E  — rewired to ROW0 pad (formerly DP)
      {3, 0},         // F
      {3, 2},         // G
      {UNAVAIL, 0},   // DP — sacrificed, ROW0 now used for E
    },
    // Digit 3 (label "3")
    {
      {4, 5},         // A
      {5, 1},         // B
      {4, 6},         // C
      {4, 1},         // D
      {4, 0},         // E  — rewired to ROW0 pad (formerly DP)
      {5, 0},         // F
      {5, 2},         // G
      {UNAVAIL, 0},   // DP — sacrificed, ROW0 now used for E
    },
    // Digit 4 — predicted from group A pattern on COM3 (bytes 6-7)
    {
      {6, 5},         // A
      {7, 1},         // B  — note: {6,4} (8D) also ghosts 4B
      {6, 6},         // C
      {6, 1},         // D
      {6, 0},         // E  — rewired to ROW0 pad (formerly DP)
      {7, 0},         // F
      {7, 2},         // G
      {UNAVAIL, 0},   // DP — sacrificed, ROW0 now used for E
    },
    // Digit 5 (label "5") — complete
    {
      {0, 2},         // A
      {0, 3},         // B
      {1, 5},         // C
      {0, 4},         // D
      {1, 3},         // E
      {1, 6},         // F
      {0, 7},         // G  — ROW7 (decoupled, E moved to ROW0)
      {1, 4},         // DP
    },
    // Digit 6 (label "6") — complete
    {
      {2, 2},         // A
      {2, 3},         // B
      {3, 5},         // C
      {2, 4},         // D
      {3, 3},         // E
      {3, 6},         // F
      {2, 7},         // G  — ROW7 (decoupled, E moved to ROW0)
      {3, 4},         // DP
    },
    // Digit 7 (label "7") — complete
    {
      {4, 2},         // A
      {4, 3},         // B
      {5, 5},         // C
      {4, 4},         // D
      {5, 3},         // E
      {5, 6},         // F
      {4, 7},         // G  — ROW7 (decoupled, E moved to ROW0)
      {5, 4},         // DP
    },
    // Digit 8
    {
      {6, 2},         // A  — wired to ROW2 pad (same as 5A/6A/7A)
      {6, 3},         // B
      {7, 5},         // C
      {6, 4},         // D  — also ghosts 4B (hardware coupled)
      {7, 3},         // E
      {7, 6},         // F
      {6, 7},         // G  — ROW7 (decoupled, E moved to ROW0)
      {7, 4},         // DP
    },
  };

  // ── Red LED → (byte, bit) — REMOVED, LEDs now on direct pins ──────────

  void cmd(uint8_t c) {
    Wire.beginTransmission(I2C_ADDR);
    Wire.write(c);
    Wire.endTransmission();
  }

  void setSegment(uint8_t digit, uint8_t seg, bool on) {
    uint8_t byte_idx = segMap_[digit][seg][0];
    if (byte_idx == UNAVAIL) return;  // segment not available
    uint8_t bit_idx = segMap_[digit][seg][1];
    if (on) buf_[byte_idx] |=  (1 << bit_idx);
    else    buf_[byte_idx] &= ~(1 << bit_idx);
  }

  void setSegments(uint8_t digit, uint8_t segs) {
    for (uint8_t s = 0; s < 8; s++) {
      uint8_t byte_idx = segMap_[digit][s][0];
      if (byte_idx == UNAVAIL) continue;
      uint8_t bit_idx = segMap_[digit][s][1];
      if (segs & (1 << s)) buf_[byte_idx] |=  (1 << bit_idx);
      else                  buf_[byte_idx] &= ~(1 << bit_idx);
    }
  }

  void setNumber(int16_t num, uint8_t startDigit, bool showLeadingZeros) {
    if (num < 0) num = 0;
    if (num > 9999) num = 9999;

    uint8_t digits[4];
    digits[3] = num % 10; num /= 10;
    digits[2] = num % 10; num /= 10;
    digits[1] = num % 10; num /= 10;
    digits[0] = num % 10;

    bool leading = true;
    for (uint8_t i = 0; i < 4; i++) {
      if (i == 3) leading = false;  // always show ones digit
      if (leading && digits[i] == 0 && !showLeadingZeros) {
        setSegments(startDigit + i, 0x00);  // blank
      } else {
        leading = false;
        setSegments(startDigit + i, FONT[digits[i]]);
      }
    }
  }
};

// Out-of-class definitions required by AVR C++11 linker
constexpr uint8_t HT16K33Display::FONT[10];
constexpr uint8_t HT16K33Display::segMap_[8][8][2];
constexpr uint8_t HT16K33Display::ledPins_[10];
