#include "animation.h"

#include <Arduino.h>

#include "rgb_panel.h"
#include "seg7.h"

namespace animation {

void startupBlink() {
  // Issue the seg7 writes first so the last I²C transaction on dig 0/1 is
  // the RGB-only word; the shared HT16K33 shadow keeps them disjoint.
  seg7::writeText(seg7::kTop, "8.8.8.8.8.");
  seg7::writeText(seg7::kBot, "8.8.8.8.8.");
  rgb_panel::setAll(true, false, false);
  delay(50);

  rgb_panel::blank();
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
}

// ── F1 starting-light state machine ────────────────────────────────────────
//
// Idle           → press a button → kLighting
// kLighting      → 5 columns of 2 LEDs light up at 1 s intervals (red);
//                  once all 10 are on we transition to kHolding
// kHolding       → wait a random 200..3000 ms with the panel still all red,
//                  then blank everything and start the reaction timer
// kReacting      → first button press stops the timer; the reaction time
//                  in ms is shown on the top 7-seg display
// kShowingResult → next button press returns to idle (ready for another go)
//
// All transitions go through a single rising-edge detector so a single
// physical press doesn't accidentally jump multiple states.
namespace {

enum class State : uint8_t {
  kIdle,
  kLighting,
  kHolding,
  kReacting,
  kShowingResult,
};

constexpr uint8_t  kColumns         = 5;     // 5 pairs = 10 LEDs
constexpr uint32_t kColumnIntervalMs = 1000; // 1 s between columns lighting
constexpr uint32_t kHoldMinMs       = 200;
constexpr uint32_t kHoldMaxMs       = 3000;

State    g_state         = State::kIdle;
uint32_t g_stateStartMs  = 0;
uint8_t  g_columnsLit    = 0;
uint32_t g_holdMs        = 0;
uint32_t g_lightsOffMs   = 0;
bool     g_prevPressed   = false;

// Bitmask of LEDs to light when `n` columns are on. Columns are paired
// left-to-right: col 0 = LEDs (#1, #2), col 1 = (#3, #4), …, col 4 = (#9, #10).
uint16_t maskForColumns(uint8_t n) {
  uint16_t out = 0;
  for (uint8_t c = 0; c < n && c < kColumns; c++) {
    out |= (1u << (2 * c)) | (1u << (2 * c + 1));
  }
  return out;
}

void renderResult(uint32_t reactionMs) {
  char buf[12];
  snprintf(buf, sizeof(buf), "%lu", (unsigned long)reactionMs);
  seg7::writeText(seg7::kTop, buf);
  seg7::clear(seg7::kBot);
  Serial.printf("F1: reaction = %lu ms\n", (unsigned long)reactionMs);
}

}  // namespace

void tick(bool pressed) {
  const uint32_t now = millis();
  const bool justPressed = pressed && !g_prevPressed;
  g_prevPressed = pressed;

  switch (g_state) {
    case State::kIdle:
      if (justPressed) {
        rgb_panel::blank();
        seg7::clear(seg7::kTop);
        seg7::clear(seg7::kBot);
        g_columnsLit  = 0;
        g_stateStartMs = now;
        g_state = State::kLighting;
        Serial.println("F1: starting sequence");
      }
      break;

    case State::kLighting: {
      // How many columns *should* be lit by now (1..kColumns).
      uint32_t elapsed = now - g_stateStartMs;
      uint8_t target = (uint8_t)(elapsed / kColumnIntervalMs) + 1;
      if (target > kColumns) target = kColumns;
      if (target != g_columnsLit) {
        g_columnsLit = target;
        rgb_panel::setRedMask(maskForColumns(g_columnsLit));
      }
      if (g_columnsLit >= kColumns &&
          now - g_stateStartMs >= kColumns * kColumnIntervalMs) {
        // All 10 LEDs are red — start the random hold.
        g_holdMs = (uint32_t)random((long)kHoldMinMs, (long)kHoldMaxMs + 1);
        g_stateStartMs = now;
        g_state = State::kHolding;
        Serial.printf("F1: holding for %lu ms\n", (unsigned long)g_holdMs);
      }
      break;
    }

    case State::kHolding:
      // Treat any press during the hold as a jump-start: skip the result,
      // bounce back to idle so the user can try again.
      if (justPressed) {
        Serial.println("F1: jump start! resetting");
        rgb_panel::blank();
        g_state = State::kIdle;
        break;
      }
      if (now - g_stateStartMs >= g_holdMs) {
        rgb_panel::blank();
        g_lightsOffMs = millis();   // re-sample for tightest timing
        g_state = State::kReacting;
        Serial.println("F1: GO!");
      }
      break;

    case State::kReacting:
      if (justPressed) {
        uint32_t reaction = millis() - g_lightsOffMs;
        renderResult(reaction);
        g_state = State::kShowingResult;
      }
      break;

    case State::kShowingResult:
      if (justPressed) {
        rgb_panel::blank();
        seg7::clear(seg7::kTop);
        seg7::clear(seg7::kBot);
        g_state = State::kIdle;
      }
      break;
  }
}

}  // namespace animation

