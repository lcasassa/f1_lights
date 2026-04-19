#include <Arduino.h>
#include "ht16k33_display.h"

HT16K33Display display;

constexpr uint8_t BTN_A = 3;
constexpr uint8_t BTN_B = 2;
constexpr uint8_t BUZZER = 5;

enum State { IDLE, READY, LIGHTING, WAIT_GO, RACE, RESULT, JUMP_START };

State state = IDLE;
uint8_t litCount = 0;
unsigned long stateTimer = 0;
unsigned long lightsOutTime = 0;
uint16_t winsA = 0;
uint16_t winsB = 0;
bool jumpA = false;
bool jumpB = false;
bool waitRelease = false;  // ignore buttons until both released
bool readyA = false;
bool readyB = false;
uint8_t borderFrame = 0;
unsigned long borderTimer = 0;

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

void showWins() {
  if (winsA > 0) display.setNumberA(winsA);
  else display.setRawA(0);
  if (winsB > 0) display.setNumberB(winsB);
  else display.setRawB(0);
}

void startSequence() {
  state = LIGHTING;
  litCount = 0;
  display.setAllLeds(false);
  showWins();
  display.write();
  stateTimer = millis() + 1000;
  jumpA = false;
  jumpB = false;
  waitRelease = true;
  Serial.println("\n--- New round ---");
  Serial.print("Score: A=");
  Serial.print(winsA);
  Serial.print(" B=");
  Serial.println(winsB);
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

  // Startup blink: all LEDs and segments on for 1s (only if a button is held)
  if (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW) {
    display.setAllLeds(true);
    for (uint8_t d = 1; d <= 8; d++) display.setRaw(d, 0xFF);
    display.write();
    delay(10000);
    display.clear();
    waitRelease = true;
  }
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
        tone(BUZZER, 600, 50);
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
        waitRelease = true;
        tone(BUZZER, 200, 500);
        display.setAllLeds(false);
        if (jumpA && !jumpB) {
          winsB++;
          display.setDashA();
          display.setNumberB(winsB);
          for (uint8_t i = 6; i <= 10; i++) display.setLed(i, true);
          Serial.println("JUMP START by Player A! Player B wins.");
        } else if (jumpB && !jumpA) {
          winsA++;
          display.setNumberA(winsA);
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
        tone(BUZZER, 200, 500);
        display.setAllLeds(false);
        if (jumpA && !jumpB) { winsB++; display.setDashA(); display.setNumberB(winsB); for (uint8_t i = 6; i <= 10; i++) display.setLed(i, true); Serial.println("JUMP START by Player A! Player B wins."); }
        else if (jumpB && !jumpA) { winsA++; display.setNumberA(winsA); display.setDashB(); for (uint8_t i = 1; i <= 5; i++) display.setLed(i, true); Serial.println("JUMP START by Player B! Player A wins."); }
        else { display.setDashA(); display.setDashB(); Serial.println("JUMP START by BOTH players! No winner."); }
        display.write();
        break;
      }

      if (now >= stateTimer) {
        // LIGHTS OUT — GO!
        display.clear();
        tone(BUZZER, 1500, 150);
        lightsOutTime = now;
        state = RACE;
        Serial.println("LIGHTS OUT! GO!");
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
          unsigned long deadline = now + 100;
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

        // Show reaction times (dash for non-presser)
        if (hitA) { display.setNumberA(reactionA > 9999 ? 9999 : reactionA, true); display.setDP(1, true); }
        else      { display.setDashA(); display.setDP(1, false); }
        if (hitB) { display.setNumberB(reactionB > 9999 ? 9999 : reactionB, true); display.setDP(5, true); }
        else      { display.setDashB(); display.setDP(5, false); }

        // Determine winner
        if (reactionA < reactionB) {
          winsA++;
          for (uint8_t i = 1; i <= 5; i++) display.setLed(i, true);
          tone(BUZZER, 1000, 80); delay(100); tone(BUZZER, 1300, 80); delay(100); tone(BUZZER, 1600, 120);
          Serial.print("Player A wins! A=");
          Serial.print(reactionA);
          Serial.print("ms B=");
          Serial.print(hitB ? reactionB : 0);
          Serial.println(hitB ? "ms" : "(no press)");
        } else if (reactionB < reactionA) {
          winsB++;
          for (uint8_t i = 6; i <= 10; i++) display.setLed(i, true);
          tone(BUZZER, 1000, 80); delay(100); tone(BUZZER, 1300, 80); delay(100); tone(BUZZER, 1600, 120);
          Serial.print("Player B wins! A=");
          Serial.print(hitA ? reactionA : 0);
          Serial.print(hitA ? "ms" : "(no press)");
          Serial.print(" B=");
          Serial.print(reactionB);
          Serial.println("ms");
        } else {
          // Tie — flash all
          display.setAllLeds(true);
          tone(BUZZER, 800, 200);
          Serial.print("TIE! Both at ");
          Serial.print(reactionA);
          Serial.println("ms");
        }
        display.write();
        Serial.print("Score: A=");
        Serial.print(winsA);
        Serial.print(" B=");
        Serial.println(winsB);

        state = RESULT;
        stateTimer = now;
        waitRelease = true;
      }

      // Timeout after 3 seconds — nobody pressed
      if (now - lightsOutTime > 3000) {
        display.setDashA();
        display.setDashB();
        display.write();
        state = RESULT;
        stateTimer = now;
        Serial.println("TIMEOUT! Nobody pressed.");
      }
      break;
    }

    case JUMP_START:
    case RESULT:
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
