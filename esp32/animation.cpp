#include "animation.h"

#include <Arduino.h>

#include "rgb_panel.h"
#include "seg7.h"

namespace animation {

void startupBlink() {
  // Issue the seg7 writes first so the last I2C transaction on dig 0/1 is
  // the RGB-only word; the shared HT16K33 shadow keeps them disjoint.
  seg7::writeText(seg7::kTop, "8.8.8.8.8.");
  seg7::writeText(seg7::kBot, "8.8.8.8.8.");
  rgb_panel::setAll(true, false, false);
  delay(50);

  rgb_panel::blank();
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
}

// F1 starting-light state machine. Two-player race timer.
//
//   kIdle           -> press either button -> kLighting (clears displays)
//   kLighting       -> 5 columns of 2 LEDs light at 1 s intervals (red);
//                      once all 10 are on we transition to kHolding
//   kHolding        -> wait random 200..3000 ms with all LEDs red; pressing
//                      during this window is a jump start (back to kIdle)
//   kReacting       -> all LEDs blank, timer running. Each player's first
//                      button press records their reaction time on their
//                      own display (A -> top, B -> bottom). Stays here
//                      until BOTH have pressed, then -> kShowingResult.
//   kShowingResult  -> results persist on the displays. Pressing either
//                      button starts a new sequence (which clears them).
//
// Rising-edge detection per button so a single physical press doesn't
// jump multiple states.
namespace {

enum class State : uint8_t {
  kIdle,
  kLighting,
  kHolding,
  kReacting,
  kShowingResult,
};

constexpr uint8_t  kColumns          = 5;
constexpr uint32_t kColumnIntervalMs = 1000;
constexpr uint32_t kHoldMinMs        = 200;
constexpr uint32_t kHoldMaxMs        = 3000;

State    g_state         = State::kIdle;
uint32_t g_stateStartMs  = 0;
uint8_t  g_columnsLit    = 0;
uint32_t g_holdMs        = 0;
uint32_t g_lightsOffMs   = 0;
bool     g_prevA         = false;
bool     g_prevB         = false;
bool     g_aRecorded     = false;
bool     g_bRecorded     = false;
uint32_t g_resultLockUntilMs = 0;   // restart is blocked while millis() < this

constexpr uint32_t kResultLockMs = 30000;   // "pin result" hold extension

// Bitmask of LEDs to light when `n` columns are on. Columns fill
// outside-in: col 0 = (#1, #10), col 1 = (#2, #9), col 2 = (#3, #8),
// col 3 = (#4, #7), col 4 = (#5, #6).
uint16_t maskForColumns(uint8_t n) {
  uint16_t out = 0;
  for (uint8_t c = 0; c < n && c < kColumns; c++) {
    out |= (1u << c) | (1u << (9 - c));
  }
  return out;
}

void writeReactionMs(const seg7::Map &display, uint32_t reactionMs, char who) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)reactionMs);
  seg7::writeText(display, buf);
  Serial.printf("F1: %c reaction = %lu ms\n", who, (unsigned long)reactionMs);
}

void startSequence(uint32_t now) {
  rgb_panel::blank();
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
  g_columnsLit        = 0;
  g_aRecorded         = false;
  g_bRecorded         = false;
  g_resultLockUntilMs = 0;
  g_stateStartMs      = now;
  g_state             = State::kLighting;
  Serial.println("F1: starting sequence");
}

}  // namespace

void tick(bool aPressed, bool bPressed) {
  const uint32_t now = millis();
  const bool aJustPressed = aPressed && !g_prevA;
  const bool bJustPressed = bPressed && !g_prevB;
  g_prevA = aPressed;
  g_prevB = bPressed;
  const bool anyJustPressed = aJustPressed || bJustPressed;

  switch (g_state) {
    case State::kIdle:
      if (anyJustPressed) startSequence(now);
      break;

    case State::kLighting: {
      uint32_t elapsed = now - g_stateStartMs;
      uint8_t target = (uint8_t)(elapsed / kColumnIntervalMs) + 1;
      if (target > kColumns) target = kColumns;
      if (target != g_columnsLit) {
        g_columnsLit = target;
        rgb_panel::setRedMask(maskForColumns(g_columnsLit));
      }
      if (g_columnsLit >= kColumns &&
          now - g_stateStartMs >= kColumns * kColumnIntervalMs) {
        g_holdMs = (uint32_t)random((long)kHoldMinMs, (long)kHoldMaxMs + 1);
        g_stateStartMs = now;
        g_state = State::kHolding;
        Serial.printf("F1: holding for %lu ms\n", (unsigned long)g_holdMs);
      }
      break;
    }

    case State::kHolding:
      if (anyJustPressed) {
        Serial.println("F1: jump start! resetting");
        rgb_panel::blank();
        g_state = State::kIdle;
        break;
      }
      if (now - g_stateStartMs >= g_holdMs) {
        rgb_panel::blank();
        g_lightsOffMs = millis();
        g_state = State::kReacting;
        Serial.println("F1: GO!");
      }
      break;

    case State::kReacting: {
      uint32_t reaction = now - g_lightsOffMs;
      if (aJustPressed && !g_aRecorded) {
        g_aRecorded = true;
        writeReactionMs(seg7::kTop, reaction, 'A');
      }
      if (bJustPressed && !g_bRecorded) {
        g_bRecorded = true;
        writeReactionMs(seg7::kBot, reaction, 'B');
      }
      if (g_aRecorded && g_bRecorded) {
        g_state = State::kShowingResult;
      }
      break;
    }

    case State::kShowingResult: {
      // Pressing BOTH buttons together (or holding them) "pins" the
      // results: the next 30 s of single-button presses are ignored, so
      // you can examine the reaction times without one accidental tap
      // wiping them. Each fresh both-press refreshes the lock window.
      if (aPressed && bPressed) {
        if (g_resultLockUntilMs - now != kResultLockMs) {  // log once per refresh
          Serial.printf("F1: result locked for %lu ms\n",
                        (unsigned long)kResultLockMs);
        }
        g_resultLockUntilMs = now + kResultLockMs;
        break;
      }
      if (now < g_resultLockUntilMs) break;       // still in lock window
      if (anyJustPressed) startSequence(now);
      break;
    }
  }
}

}  // namespace animation






