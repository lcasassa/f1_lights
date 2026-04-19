#include <Arduino.h>
#include "ht16k33_display.h"

HT16K33Display display;

constexpr uint8_t BTN_A = 3;
constexpr uint8_t BTN_B = 2;
constexpr uint8_t BUZZER = 5;

enum State { IDLE, LIGHTING, WAIT_GO, RACE, RESULT, JUMP_START };

State state = IDLE;
uint8_t litCount = 0;
unsigned long stateTimer = 0;
unsigned long lightsOutTime = 0;
uint16_t winsA = 0;
uint16_t winsB = 0;
bool jumpA = false;
bool jumpB = false;
bool waitRelease = false;  // ignore buttons until both released

void showWins() {
  display.setNumberA(winsA);
  display.setNumberB(winsB);
}

void startSequence() {
  state = LIGHTING;
  litCount = 0;
  display.setAllLeds(false);
  showWins();
  display.write();
  stateTimer = millis() + 1000;  // pause before first light
  jumpA = false;
  jumpB = false;
  waitRelease = true;  // don't check buttons until released
}

void setup() {
  Serial.begin(9600);
  Serial.println("\n=== F1 LIGHTS OUT! ===");
  Serial.println("Press any button to start.");
  Serial.println("When lights go out, press fastest!\n");

  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);

  display.begin();
  randomSeed(analogRead(A7));  // seed from floating analog pin
  display.setAllLeds(false);
  showWins();
  display.write();
}

void loop() {
  bool btnA = digitalRead(BTN_A) == LOW;
  bool btnB = digitalRead(BTN_B) == LOW;
  unsigned long now = millis();

  // After starting a round, ignore buttons until both are released
  if (waitRelease) {
    if (!btnA && !btnB) {
      waitRelease = false;
    }
    btnA = false;
    btnB = false;
  }

  switch (state) {

    case IDLE:
      // Any button starts the round
      if (btnA || btnB) {
        startSequence();
      }
      break;

    case LIGHTING:
      // Check for jump start
      if (btnA) { jumpA = true; }
      if (btnB) { jumpB = true; }
      if (jumpA || jumpB) {
        state = JUMP_START;
        stateTimer = now;
        waitRelease = true;
        tone(BUZZER, 200, 500);  // harsh buzz for foul
        // Flash all LEDs to indicate foul
        display.setAllLeds(true);
        if (jumpA && !jumpB) {
          winsB++;  // B wins by default
          display.setDashA();
          display.setNumberB(winsB);
        } else if (jumpB && !jumpA) {
          winsA++;  // A wins by default
          display.setNumberA(winsA);
          display.setDashB();
        } else {
          // Both jump = no winner, show dashes on both
          display.setDashA();
          display.setDashB();
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
        tone(BUZZER, 600, 50);  // short beep per pair

        if (litCount >= 5) {
          // All 5 pairs lit — wait random 1-4s then go dark
          state = WAIT_GO;
          stateTimer = now + 1000 + (random(3000));
        } else {
          stateTimer = now + 800;  // next pair in 800ms
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
        waitRelease = true;
        tone(BUZZER, 200, 500);  // harsh buzz for foul
        display.setAllLeds(true);
        if (jumpA && !jumpB) { winsB++; display.setDashA(); display.setNumberB(winsB); }
        else if (jumpB && !jumpA) { winsA++; display.setNumberA(winsA); display.setDashB(); }
        else { display.setDashA(); display.setDashB(); }
        display.write();
        break;
      }

      if (now >= stateTimer) {
        // LIGHTS OUT — GO!
        display.clear();  // clears entire buffer and writes
        tone(BUZZER, 1500, 150);  // GO beep
        lightsOutTime = now;
        state = RACE;
      }
      break;

    case RACE: {
      bool hitA = btnA;
      bool hitB = btnB;

      if (hitA || hitB) {
        uint16_t reactionA = hitA ? (uint16_t)(now - lightsOutTime) : 9999;
        uint16_t reactionB = hitB ? (uint16_t)(now - lightsOutTime) : 9999;

        // Wait briefly for the other player
        if (!hitA || !hitB) {
          unsigned long deadline = now + 500;
          while (millis() < deadline) {
            if (!hitA && digitalRead(BTN_A) == LOW) {
              hitA = true;
              reactionA = (uint16_t)(millis() - lightsOutTime);
            }
            if (!hitB && digitalRead(BTN_B) == LOW) {
              hitB = true;
              reactionB = (uint16_t)(millis() - lightsOutTime);
            }
          }
        }

        // Show reaction times
        display.setNumberA(reactionA > 9999 ? 9999 : reactionA);
        display.setNumberB(reactionB > 9999 ? 9999 : reactionB);

        // Determine winner
        if (reactionA < reactionB) {
          winsA++;
          for (uint8_t i = 1; i <= 5; i++) display.setLed(i, true);
        } else if (reactionB < reactionA) {
          winsB++;
          for (uint8_t i = 6; i <= 10; i++) display.setLed(i, true);
        } else {
          // Tie — flash all
          display.setAllLeds(true);
        }
        display.write();

        state = RESULT;
        stateTimer = now;
        waitRelease = true;
      }

      // Timeout after 3 seconds — nobody pressed
      if (now - lightsOutTime > 3000) {
        display.setNumberA(9999);
        display.setNumberB(9999);
        display.write();
        state = RESULT;
        stateTimer = now;
      }
      break;
    }

    case JUMP_START:
    case RESULT:
      // Show result for 5s, then back to idle showing wins
      if (now - stateTimer > 5000) {
        display.setAllLeds(false);
        showWins();
        display.write();
        state = IDLE;
      }
      break;
  }
}
