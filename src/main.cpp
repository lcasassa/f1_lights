#include <Arduino.h>
#include "ht16k33_display.h"

HT16K33Display display;

constexpr uint8_t BTN_A = 3;
constexpr uint8_t BTN_B = 2;
constexpr uint8_t BUZZER = 5;

enum State { IDLE, READY, LIGHTING, WAIT_GO, RACE, RACE_GRACE, RESULT, JUMP_START };

State state = IDLE;
uint8_t litCount = 0;
unsigned long stateTimer = 0;
unsigned long lightsOutTime = 0;
bool jumpA = false;
bool jumpB = false;
bool waitReleaseA = false;  // ignore button A until released
bool waitReleaseB = false;  // ignore button B until released
bool readyA = false;
bool readyB = false;
uint8_t borderFrame = 0;
unsigned long borderTimer = 0;

// RACE grace window (non-blocking 100ms wait for second player)
constexpr unsigned long RACE_GRACE_MS = 100;
unsigned long raceGraceStart = 0;
bool raceHitA = false;
bool raceHitB = false;
uint16_t raceReactionA = 9999;
uint16_t raceReactionB = 9999;

// Minimum time to display results before accepting restart
constexpr unsigned long RESULT_DISPLAY_MS = 2000;

// Debounce state — instant press, delayed release
constexpr unsigned long DEBOUNCE_MS = 20;
bool stableA = false, stableB = false;
unsigned long releaseA = 0, releaseB = 0;

// Border animation: chase around the perimeter of all 4 digits
// Each entry is {digit_1based, segment_mask}
struct BorderStep { uint8_t digit; uint8_t seg; };
const BorderStep BORDER_SEQ[] = {
  {1, HT16K33Display::A},  // top of digit 1
  {2, HT16K33Display::A},  // top of digit 2
  {3, HT16K33Display::A},  // top of digit 3
  {4, HT16K33Display::A},  // top of digit 4
  {4, HT16K33Display::B},  // right-top of digit 4
  {4, HT16K33Display::C},  // right-bottom of digit 4
  {4, HT16K33Display::D},  // bottom of digit 4
  {3, HT16K33Display::D},  // bottom of digit 3
  {2, HT16K33Display::D},  // bottom of digit 2
  {1, HT16K33Display::D},  // bottom of digit 1
  {1, HT16K33Display::E},  // left-bottom of digit 1
  {1, HT16K33Display::F},  // left-top of digit 1
};
constexpr uint8_t BORDER_LEN = 12;
constexpr unsigned long BORDER_MS = 60;

void startSequence() {
  state = LIGHTING;
  litCount = 0;
  display.setAllLeds(false);
  display.setRawA(0);
  display.setRawB(0);
  display.write();
  stateTimer = millis() + 1000;
  jumpA = false;
  jumpB = false;
  waitReleaseA = true; waitReleaseB = true;
  Serial.println("\n--- New round ---");
}

void setup() {
  // Reset all game state (important for sim where setup() may be called multiple times)
  state = IDLE;
  litCount = 0;
  stateTimer = 0;
  lightsOutTime = 0;
  jumpA = false;
  jumpB = false;
  waitReleaseA = false;
  waitReleaseB = false;
  readyA = false;
  readyB = false;
  borderFrame = 0;
  borderTimer = 0;
  raceGraceStart = 0;
  raceHitA = false;
  raceHitB = false;
  raceReactionA = 9999;
  raceReactionB = 9999;
  stableA = false;
  stableB = false;
  releaseA = 0;
  releaseB = 0;

  Serial.begin(115200);
  Serial.println("\n=== F1 LIGHTS OUT! ===");
  Serial.println("Press any button to start.");
  Serial.println("When lights go out, press fastest!\n");

  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  display.begin();
  randomSeed(analogRead(A7));  // seed from floating analog pin

  // Startup flash: all LEDs + all segments on, short beep
  display.setAllLeds(true);
  for (uint8_t d = 1; d <= 8; d++) display.setRaw(d, 0xFF);
  display.write();
  tone(BUZZER, 1200, 20);
  delay(300);
  display.setAllLeds(false);
  display.clear();

  // Start in READY state so the "waiting for players" animation plays immediately
  state = READY;
  readyA = false;
  readyB = false;
  borderFrame = 0;
  borderTimer = millis();

  // Interactive segment debug: both buttons held at startup
  if (digitalRead(BTN_A) == LOW && digitalRead(BTN_B) == LOW) {
    // 8 digits × 9 steps (A,B,C,D,E,F,G,DP, all-on) = 72 total steps
    static const char* SEG_NAMES[] = {"A","B","C","D","E","F","G","DP","ALL"};
    static const uint8_t SEG_VALS[] = {
      HT16K33Display::A, HT16K33Display::B, HT16K33Display::C,
      HT16K33Display::D, HT16K33Display::E, HT16K33Display::F,
      HT16K33Display::G, HT16K33Display::DP, 0xFF
    };
    constexpr uint8_t NUM_SEGS = 9;
    constexpr uint8_t TOTAL = 8 * NUM_SEGS;  // 72

    int16_t step = 0;
    bool prevA = true, prevB = true;  // buttons currently held

    auto showStep = [&]() {
      uint8_t digit = (step / NUM_SEGS) + 1;  // 1-8
      uint8_t seg   = step % NUM_SEGS;         // 0-8
      uint8_t segs  = (SEG_VALS[seg] == 0xFF) ? 0xFF : SEG_VALS[seg];

      // Update display: only this digit lit
      for (uint8_t o = 1; o <= 8; o++) display.setRaw(o, 0);
      display.setRaw(digit, segs);
      display.write();

      // Print to Serial
      uint8_t byteIdx, bitIdx;
      display.getSegMap(digit - 1, seg < 8 ? seg : 0, byteIdx, bitIdx);

      Serial.print(F("Step "));
      Serial.print(step);
      Serial.print(F("  Digit "));
      Serial.print(digit);
      Serial.print(F("  Seg "));
      Serial.print(SEG_NAMES[seg]);
      if (seg < 8) {
        Serial.print(F("  byte="));
        Serial.print(byteIdx);
        Serial.print(F(" bit="));
        Serial.print(bitIdx);
        if (byteIdx == 0xFF) Serial.print(F(" [UNAVAIL]"));
      }
      Serial.println();
    };

    // Wait for both buttons released first
    while (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW) {}
    delay(50);

    Serial.println(F("\n=== SEGMENT DEBUG MODE ==="));
    Serial.println(F("BTN_A (pin 3) = next, BTN_B (pin 2) = prev"));
    Serial.println(F("Press both to exit\n"));

    showStep();

    while (true) {
      bool a = digitalRead(BTN_A) == LOW;
      bool b = digitalRead(BTN_B) == LOW;

      // Exit on both pressed
      if (a && b) break;

      // Detect rising edge (button press)
      if (a && !prevA) {
        step = (step + 1) % TOTAL;
        showStep();
      }
      if (b && !prevB) {
        step = (step - 1 + TOTAL) % TOTAL;
        showStep();
      }

      prevA = a;
      prevB = b;
      delay(30);  // debounce
    }

    Serial.println(F("\n=== EXIT DEBUG MODE ===\n"));
    display.clear();
    waitReleaseA = true; waitReleaseB = true;
    // Restart in READY state after debug
    state = READY;
    readyA = false;
    readyB = false;
    borderFrame = 0;
    borderTimer = millis();
  }
}

void loop() {
  unsigned long now = millis();

  // Debounce: act on press instantly, delay release by DEBOUNCE_MS
  bool readA = digitalRead(BTN_A) == LOW;
  bool readB = digitalRead(BTN_B) == LOW;

  if (readA) { stableA = true;  releaseA = now; }
  else if (now - releaseA >= DEBOUNCE_MS) { stableA = false; }

  if (readB) { stableB = true;  releaseB = now; }
  else if (now - releaseB >= DEBOUNCE_MS) { stableB = false; }

  bool btnA = stableA;
  bool btnB = stableB;

  // After starting a round, ignore each button until it's released
  // (RESULT, JUMP_START, and RACE_GRACE handle waitRelease internally)
  if (state != RESULT && state != JUMP_START && state != RACE_GRACE) {
    if (waitReleaseA) {
      if (!btnA) waitReleaseA = false;
      btnA = false;
    }
    if (waitReleaseB) {
      if (!btnB) waitReleaseB = false;
      btnB = false;
    }
  }

  switch (state) {

    case IDLE:
      // Any button enters READY mode
      if (btnA || btnB) {
        readyA = btnA;
        readyB = btnB;
        borderFrame = 0;
        borderTimer = now;
        state = READY;
      }
      break;

    case READY: {
      // First press from a new player
      if (btnA && !readyA) { readyA = true; }
      if (btnB && !readyB) { readyB = true; }

      // Animate border on ready player's digits
      if (now - borderTimer >= BORDER_MS) {
        borderTimer = now;
        borderFrame = (borderFrame + 1) % BORDER_LEN;

        // Clear digits then draw current + tail (comet effect)
        if (!readyA) {
          for (uint8_t d = 1; d <= 4; d++) display.setRaw(d, 0);
          const BorderStep &cur  = BORDER_SEQ[borderFrame];
          const BorderStep &tail = BORDER_SEQ[(borderFrame + BORDER_LEN - 1) % BORDER_LEN];
          display.setRaw(cur.digit, cur.seg);
          if (tail.digit == cur.digit)
            display.setRaw(cur.digit, cur.seg | tail.seg);
          else
            display.setRaw(tail.digit, tail.seg);
        } else {
          display.setRawA(HT16K33Display::D);
        }

        if (!readyB) {
          for (uint8_t d = 5; d <= 8; d++) display.setRaw(d, 0);
          const BorderStep &cur  = BORDER_SEQ[borderFrame];
          const BorderStep &tail = BORDER_SEQ[(borderFrame + BORDER_LEN - 1) % BORDER_LEN];
          display.setRaw(cur.digit + 4, cur.seg);
          if (tail.digit == cur.digit)
            display.setRaw(cur.digit + 4, cur.seg | tail.seg);
          else
            display.setRaw(tail.digit + 4, tail.seg);
        } else {
          display.setRawB(HT16K33Display::D);
        }

        display.write();
      }

      // Both ready → start the race
      if (readyA && readyB) {
        Serial.println("Both players ready!");
        startSequence();
        litCount = 1;  // first pair already lit
        display.setLed(1, true);
        display.setLed(6, true);
        tone(BUZZER, 1000, 200);  // F1-style beep: 1kHz, 200ms
        display.write();
      }
      break;
    }

    case LIGHTING:
      // Check for jump start
      if (btnA) { jumpA = true; }
      if (btnB) { jumpB = true; }
      if (jumpA || jumpB) {
        state = JUMP_START;
        stateTimer = now;
        waitReleaseA = true; waitReleaseB = true;
        tone(BUZZER, 200, 500);
        display.setAllLeds(false);
        if (jumpA && !jumpB) {
          display.setDashA();
          for (uint8_t i = 6; i <= 10; i++) display.setLed(i, true);
          Serial.println("JUMP START by Player A! Player B wins.");
        } else if (jumpB && !jumpA) {
          display.setDashB();
          for (uint8_t i = 1; i <= 5; i++) display.setLed(i, true);
          Serial.println("JUMP START by Player B! Player A wins.");
        } else {
          display.setDashA();
          display.setDashB();
          Serial.println("JUMP START by BOTH players! No winner.");
        }
        display.write();
        break;
      }

      if (now >= stateTimer) {
        litCount++;
        // Light LEDs in pairs: L1+L6, L2+L7, L3+L8, L4+L9, L5+L10
        display.setLed(litCount, true);
        display.setLed(litCount + 5, true);
        display.write();
        tone(BUZZER, 1000, 200);  // F1-style beep: 1kHz, 200ms

        if (litCount >= 5) {
          // All 5 pairs lit — wait random 1-4s then go dark
          state = WAIT_GO;
          stateTimer = now + 1000 + (random(3000));
        } else {
          stateTimer = now + 1000;  // next pair in 1s (F1 standard)
        }
      }
      break;

    case WAIT_GO:
      // Check for jump start during wait
      if (btnA) { jumpA = true; }
      if (btnB) { jumpB = true; }
      if (jumpA || jumpB) {
        state = JUMP_START;
        stateTimer = now;
        waitReleaseA = true; waitReleaseB = true;
        tone(BUZZER, 200, 500);
        display.setAllLeds(false);
        if (jumpA && !jumpB) { display.setDashA(); for (uint8_t i = 6; i <= 10; i++) display.setLed(i, true); Serial.println("JUMP START by Player A! Player B wins."); }
        else if (jumpB && !jumpA) { display.setDashB(); for (uint8_t i = 1; i <= 5; i++) display.setLed(i, true); Serial.println("JUMP START by Player B! Player A wins."); }
        else { display.setDashA(); display.setDashB(); Serial.println("JUMP START by BOTH players! No winner."); }
        display.write();
        break;
      }

      if (now >= stateTimer) {
        // LIGHTS OUT — GO!
        display.setAllLeds(false);
        display.clear();
        noTone(BUZZER);  // silence — in F1, lights out is purely visual
        lightsOutTime = now;
        state = RACE;
        Serial.println("LIGHTS OUT! GO!");
      }
      break;

    case RACE: {
      bool hitA = btnA;
      bool hitB = btnB;

      if (hitA || hitB) {
        // Record first press(es)
        raceHitA = hitA;
        raceHitB = hitB;
        raceReactionA = hitA ? (uint16_t)(now - lightsOutTime) : 9999;
        raceReactionB = hitB ? (uint16_t)(now - lightsOutTime) : 9999;

        if (raceHitA && raceHitB) {
          // Both pressed on the same tick — skip grace window
          state = RACE_GRACE;
          raceGraceStart = now - RACE_GRACE_MS;  // immediately expire
        } else {
          // One pressed — start non-blocking grace window for the other
          state = RACE_GRACE;
          raceGraceStart = now;
        }
      }

      // Timeout after 3 seconds — nobody pressed
      if (now - lightsOutTime > 3000) {
        display.setDashA();
        display.setDashB();
        display.write();
        state = RESULT;
        stateTimer = now;
        waitReleaseA = true; waitReleaseB = true;
        Serial.println("TIMEOUT! Nobody pressed.");
      }
      break;
    }

    case RACE_GRACE: {
      // Non-blocking 100ms grace window: check if the other player also pressed
      if (!raceHitA && btnA) {
        raceHitA = true;
        raceReactionA = (uint16_t)(now - lightsOutTime);
      }
      if (!raceHitB && btnB) {
        raceHitB = true;
        raceReactionB = (uint16_t)(now - lightsOutTime);
      }

      // Grace period expired (or both pressed) — show results
      if ((now - raceGraceStart >= RACE_GRACE_MS) || (raceHitA && raceHitB)) {
        // Show reaction times (dash for non-presser)
        if (raceHitA) { display.setNumberA(raceReactionA > 9999 ? 9999 : raceReactionA, true); display.setDP(1, true); }
        else          { display.setDashA(); display.setDP(1, false); }
        if (raceHitB) { display.setNumberB(raceReactionB > 9999 ? 9999 : raceReactionB, true); display.setDP(5, true); }
        else          { display.setDashB(); display.setDP(5, false); }

        // Determine winner
        if (raceReactionA < raceReactionB) {
          for (uint8_t i = 1; i <= 5; i++) display.setLed(i, true);
          tone(BUZZER, 1000, 80); delay(100); tone(BUZZER, 1300, 80); delay(100); tone(BUZZER, 1600, 120);
          Serial.print("Player A wins! A=");
          Serial.print(raceReactionA);
          Serial.print("ms B=");
          Serial.print(raceHitB ? raceReactionB : 0);
          Serial.println(raceHitB ? "ms" : "(no press)");
        } else if (raceReactionB < raceReactionA) {
          for (uint8_t i = 6; i <= 10; i++) display.setLed(i, true);
          tone(BUZZER, 1000, 80); delay(100); tone(BUZZER, 1300, 80); delay(100); tone(BUZZER, 1600, 120);
          Serial.print("Player B wins! A=");
          Serial.print(raceHitA ? raceReactionA : 0);
          Serial.print(raceHitA ? "ms" : "(no press)");
          Serial.print(" B=");
          Serial.print(raceReactionB);
          Serial.println("ms");
        } else {
          // Tie — flash all
          display.setAllLeds(true);
          tone(BUZZER, 800, 200);
          Serial.print("TIE! Both at ");
          Serial.print(raceReactionA);
          Serial.println("ms");
        }
        display.write();

        state = RESULT;
        stateTimer = millis();  // use millis() since delay() may have advanced clock
        waitReleaseA = true; waitReleaseB = true;
      }
      break;
    }

    case JUMP_START:
    case RESULT:
      // Wait for BOTH buttons to be released first
      if (waitReleaseA && !btnA) waitReleaseA = false;
      if (waitReleaseB && !btnB) waitReleaseB = false;
      if (waitReleaseA || waitReleaseB) break;

      // Enforce minimum display time before accepting restart
      if (now - stateTimer < RESULT_DISPLAY_MS) break;

      // Show result until a player presses a button — that press also counts as ready
      if (btnA || btnB) {
        display.setAllLeds(false);
        display.clear();
        readyA = btnA;
        readyB = btnB;
        borderFrame = 0;
        borderTimer = now;
        state = READY;
      }
      break;
  }
}
