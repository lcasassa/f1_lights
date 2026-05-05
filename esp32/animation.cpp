#include "animation.h"

#include <Arduino.h>

#include "peripherals.h"
#include "rgb_panel.h"
#include "seg7.h"

namespace animation {

void startupBlink() {
  // Issue the seg7 writes first so the last I2C transaction on dig 0/1 is
  // the RGB-only word; the shared HT16K33 shadow keeps them disjoint.
  seg7::writeText(seg7::kTop, "8.8.8.8.8.");
  seg7::writeText(seg7::kBot, "8.8.8.8.8.");
  rgb_panel::setAll(true, false, false);
  peripherals::buzzerBeep(1200, 20);
  delay(50);

  rgb_panel::blank();
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
}

// Two-player F1 starting-light reaction game, ported from src/main.cpp.
//
//   IDLE        -> any button -> READY (waiting-for-players animation)
//   READY       -> per-player "I'm in" press latches that side; once both
//                  are latched -> LIGHTING (column 1 lit immediately)
//   LIGHTING    -> 4 more pairs of LEDs light at 1 s intervals; press
//                  during this window = JUMP_START
//   WAIT_GO     -> all 10 lit, random 1..4 s hold; press here also =
//                  JUMP_START. Hold expires -> RACE (lights out)
//   RACE        -> first press starts a 100 ms grace window for the other
//                  player to also press -> RACE_GRACE. 3 s with no press
//                  -> timeout, both displays show "-----".
//   RACE_GRACE  -> after grace window (or both pressed), reaction times
//                  are written to each player's display; winner LEDs +
//                  victory beep -> RESULT
//   JUMP_START  -> dashes/LEDs show who jumped; loser side lit
//   RESULT      -> wait both released, enforce minimum display time,
//                  next single press restarts in READY (with that side
//                  already latched)
namespace {

enum State : uint8_t {
  IDLE,
  READY,
  LIGHTING,
  WAIT_GO,
  RACE,
  RACE_GRACE,
  RESULT,
  JUMP_START,
};

constexpr uint32_t kRaceGraceMs       = 100;
constexpr uint32_t kResultDisplayMs   = 2000;
constexpr uint32_t kDebounceMs        = 20;
constexpr uint32_t kBorderMs          = 120;
constexpr uint32_t kRaceTimeoutMs     = 3000;
constexpr uint8_t  kBorderLen         = 5;        // cells per panel

// Game state
State    g_state          = IDLE;
uint8_t  g_litCount       = 0;
uint32_t g_stateTimer     = 0;
uint32_t g_lightsOutTime  = 0;
bool     g_jumpA          = false;
bool     g_jumpB          = false;
bool     g_waitReleaseA   = false;
bool     g_waitReleaseB   = false;
bool     g_readyA         = false;
bool     g_readyB         = false;
uint8_t  g_borderFrame    = 0;
uint32_t g_borderTimer    = 0;
uint16_t g_ledMask        = 0;

// Race grace window
uint32_t g_raceGraceStart = 0;
bool     g_raceHitA       = false;
bool     g_raceHitB       = false;
uint16_t g_raceReactionA  = 9999;
uint16_t g_raceReactionB  = 9999;

// Debounce state — instant press, delayed release
bool     g_stableA        = false;
bool     g_stableB        = false;
uint32_t g_releaseA       = 0;
uint32_t g_releaseB       = 0;

// LEDs are lit in pairs: column k (1..5) -> LED k and LED k+5.
// Bit i in the mask = LED (i+1).
void setLed(uint8_t led1based, bool on) {
  if (led1based < 1 || led1based > 10) return;
  uint16_t bit = (uint16_t)(1u << (led1based - 1));
  if (on) g_ledMask |= bit;
  else    g_ledMask &= (uint16_t)~bit;
  rgb_panel::setRedMask(g_ledMask);
}

void setAllLeds(bool on) {
  g_ledMask = on ? 0x3FFu : 0u;
  if (on) rgb_panel::setRedMask(g_ledMask);
  else    rgb_panel::blank();
}

void clearDisplays() {
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
}

// Render a reaction time (ms, 0..9999) right-aligned. We have 5 cells, so
// just print the integer; no DP gymnastics needed.
void showReaction(const seg7::Map &m, uint16_t ms, char who) {
  char buf[8];
  if (ms > 9999) ms = 9999;
  snprintf(buf, sizeof(buf), "%u", (unsigned)ms);
  seg7::writeText(m, buf);
  Serial.printf("F1: %c reaction = %u ms\n", who, (unsigned)ms);
}

void showDash(const seg7::Map &m) {
  seg7::writeText(m, "-----");
}

// Chasing dash across the 5 cells of one panel (replaces the
// raw-segment perimeter comet from the Arduino build, since the seg7
// helper only exposes character writes).
void drawBorderFrame(const seg7::Map &m, uint8_t frame) {
  char buf[6] = "     ";
  if (frame < kBorderLen) buf[frame] = '-';
  buf[5] = 0;
  seg7::writeText(m, buf);
}

void startSequence(uint32_t now) {
  g_state = LIGHTING;
  g_litCount = 0;
  setAllLeds(false);
  clearDisplays();
  g_stateTimer = now + 1000;
  g_jumpA = false;
  g_jumpB = false;
  g_waitReleaseA = true;
  g_waitReleaseB = true;
  Serial.println("\nF1: --- New round ---");
}

void enterJumpStart(uint32_t now) {
  g_state = JUMP_START;
  g_stateTimer = now;
  g_waitReleaseA = true;
  g_waitReleaseB = true;
  setAllLeds(false);
  if (g_jumpA && !g_jumpB) {
    showDash(seg7::kTop);
    seg7::clear(seg7::kBot);
    // Loser top, winner = B → light B's LEDs (6..10)
    for (uint8_t i = 6; i <= 10; i++) setLed(i, true);
    Serial.println("F1: JUMP START by Player A! Player B wins.");
  } else if (g_jumpB && !g_jumpA) {
    showDash(seg7::kBot);
    seg7::clear(seg7::kTop);
    for (uint8_t i = 1; i <= 5; i++) setLed(i, true);
    Serial.println("F1: JUMP START by Player B! Player A wins.");
  } else {
    showDash(seg7::kTop);
    showDash(seg7::kBot);
    Serial.println("F1: JUMP START by BOTH players! No winner.");
  }
  peripherals::buzzerBeep(200, 500);
}

}  // namespace

void tick(bool aPressed, bool bPressed) {
  const uint32_t now = millis();

  // Debounce: act on press instantly, delay release by kDebounceMs
  if (aPressed) { g_stableA = true; g_releaseA = now; }
  else if (now - g_releaseA >= kDebounceMs) { g_stableA = false; }

  if (bPressed) { g_stableB = true; g_releaseB = now; }
  else if (now - g_releaseB >= kDebounceMs) { g_stableB = false; }

  bool btnA = g_stableA;
  bool btnB = g_stableB;

  // After starting a round, ignore each button until it's released
  // (RESULT, JUMP_START, RACE_GRACE manage waitRelease internally).
  if (g_state != RESULT && g_state != JUMP_START && g_state != RACE_GRACE) {
    if (g_waitReleaseA) {
      if (!btnA) g_waitReleaseA = false;
      btnA = false;
    }
    if (g_waitReleaseB) {
      if (!btnB) g_waitReleaseB = false;
      btnB = false;
    }
  }

  switch (g_state) {

    case IDLE:
      if (btnA || btnB) {
        g_readyA = btnA;
        g_readyB = btnB;
        g_borderFrame = 0;
        g_borderTimer = now;
        g_state = READY;
      }
      break;

    case READY: {
      if (btnA && !g_readyA) g_readyA = true;
      if (btnB && !g_readyB) g_readyB = true;

      if (now - g_borderTimer >= kBorderMs) {
        g_borderTimer = now;
        g_borderFrame = (uint8_t)((g_borderFrame + 1) % kBorderLen);

        if (!g_readyA) drawBorderFrame(seg7::kTop, g_borderFrame);
        else           seg7::clear(seg7::kTop);

        if (!g_readyB) drawBorderFrame(seg7::kBot, g_borderFrame);
        else           seg7::clear(seg7::kBot);
      }

      if (g_readyA && g_readyB) {
        Serial.println("F1: Both players ready!");
        startSequence(now);
        // First pair already lit (matches src/main.cpp behaviour).
        g_litCount = 1;
        setLed(1, true);
        setLed(6, true);
        peripherals::buzzerBeep(1000, 200);
      }
      break;
    }

    case LIGHTING:
      if (btnA) g_jumpA = true;
      if (btnB) g_jumpB = true;
      if (g_jumpA || g_jumpB) { enterJumpStart(now); break; }

      if (now >= g_stateTimer) {
        g_litCount++;
        setLed(g_litCount, true);
        setLed(g_litCount + 5, true);
        peripherals::buzzerBeep(1000, 200);

        if (g_litCount >= 5) {
          g_state = WAIT_GO;
          g_stateTimer = now + 1000 + (uint32_t)random(3000);
        } else {
          g_stateTimer = now + 1000;
        }
      }
      break;

    case WAIT_GO:
      if (btnA) g_jumpA = true;
      if (btnB) g_jumpB = true;
      if (g_jumpA || g_jumpB) { enterJumpStart(now); break; }

      if (now >= g_stateTimer) {
        // LIGHTS OUT — GO!
        setAllLeds(false);
        clearDisplays();
        g_lightsOutTime = now;
        g_state = RACE;
        Serial.println("F1: LIGHTS OUT! GO!");
      }
      break;

    case RACE: {
      if (btnA || btnB) {
        g_raceHitA = btnA;
        g_raceHitB = btnB;
        g_raceReactionA = btnA ? (uint16_t)(now - g_lightsOutTime) : 9999;
        g_raceReactionB = btnB ? (uint16_t)(now - g_lightsOutTime) : 9999;

        if (g_raceHitA && g_raceHitB) {
          // Both pressed on the same tick — skip grace window.
          g_state = RACE_GRACE;
          g_raceGraceStart = now - kRaceGraceMs;
        } else {
          g_state = RACE_GRACE;
          g_raceGraceStart = now;
        }
      }

      if (now - g_lightsOutTime > kRaceTimeoutMs) {
        showDash(seg7::kTop);
        showDash(seg7::kBot);
        g_state = RESULT;
        g_stateTimer = now;
        g_waitReleaseA = true;
        g_waitReleaseB = true;
        Serial.println("F1: TIMEOUT! Nobody pressed.");
      }
      break;
    }

    case RACE_GRACE: {
      if (!g_raceHitA && btnA) {
        g_raceHitA = true;
        g_raceReactionA = (uint16_t)(now - g_lightsOutTime);
      }
      if (!g_raceHitB && btnB) {
        g_raceHitB = true;
        g_raceReactionB = (uint16_t)(now - g_lightsOutTime);
      }

      if ((now - g_raceGraceStart >= kRaceGraceMs) ||
          (g_raceHitA && g_raceHitB)) {
        if (g_raceHitA) showReaction(seg7::kTop, g_raceReactionA, 'A');
        else            showDash(seg7::kTop);
        if (g_raceHitB) showReaction(seg7::kBot, g_raceReactionB, 'B');
        else            showDash(seg7::kBot);

        if (g_raceReactionA < g_raceReactionB) {
          for (uint8_t i = 1; i <= 5; i++) setLed(i, true);
          peripherals::buzzerBeep(1000, 80);
          peripherals::buzzerBeep(1300, 80);
          peripherals::buzzerBeep(1600, 120);
          Serial.printf("F1: Player A wins! A=%u ms B=%s\n",
                        (unsigned)g_raceReactionA,
                        g_raceHitB ? String((unsigned)g_raceReactionB).c_str()
                                   : "(no press)");
        } else if (g_raceReactionB < g_raceReactionA) {
          for (uint8_t i = 6; i <= 10; i++) setLed(i, true);
          peripherals::buzzerBeep(1000, 80);
          peripherals::buzzerBeep(1300, 80);
          peripherals::buzzerBeep(1600, 120);
          Serial.printf("F1: Player B wins! A=%s B=%u ms\n",
                        g_raceHitA ? String((unsigned)g_raceReactionA).c_str()
                                   : "(no press)",
                        (unsigned)g_raceReactionB);
        } else {
          setAllLeds(true);
          peripherals::buzzerBeep(800, 200);
          Serial.printf("F1: TIE! Both at %u ms\n",
                        (unsigned)g_raceReactionA);
        }

        g_state = RESULT;
        g_stateTimer = millis();   // delays in buzzer may have advanced clock
        g_waitReleaseA = true;
        g_waitReleaseB = true;
      }
      break;
    }

    case JUMP_START:
    case RESULT:
      // Wait for both buttons to be released first.
      if (g_waitReleaseA && !btnA) g_waitReleaseA = false;
      if (g_waitReleaseB && !btnB) g_waitReleaseB = false;
      if (g_waitReleaseA || g_waitReleaseB) break;

      // Enforce minimum display time before accepting restart.
      if (now - g_stateTimer < kResultDisplayMs) break;

      if (btnA || btnB) {
        setAllLeds(false);
        clearDisplays();
        g_readyA = btnA;
        g_readyB = btnB;
        g_borderFrame = 0;
        g_borderTimer = now;
        g_state = READY;
      }
      break;
  }
}

}  // namespace animation

