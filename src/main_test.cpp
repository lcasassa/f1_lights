#include <Arduino.h>

namespace {
// F1 Start Light Configuration
// Logical LED positions: 1-5 (top row), 6-10 (bottom row)
// Map each logical position to the actual Arduino pin it's connected to
constexpr uint8_t PIN_MAP[] = {
  13,  // POS-1  (Row 1, leftmost)
  4,   // POS-2  (Row 1)
  12,  // POS-3  (Row 1)
  11,  // POS-4  (Row 1)
  10,  // POS-5  (Row 1, rightmost)
  5,   // POS-6  (Row 2, leftmost)
  6,   // POS-7  (Row 2)
  7,   // POS-8  (Row 2)
  8,   // POS-9  (Row 2)
  9    // POS-10 (Row 2, rightmost)
};

constexpr uint8_t NUM_LEDS = 10;

// Button Configuration
constexpr uint8_t BUTTON_LEFT = 2;                     // Left button on pin 2 (pull-up)
constexpr uint8_t BUTTON_RIGHT = 3;                    // Right button on pin 3 (pull-up)
constexpr unsigned long BUTTON_DEBOUNCE_MS = 10;        // Debounce time in milliseconds

// Hardware Test Sequence
constexpr unsigned long LED_ON_TIME_MS = 500;           // Each LED on for 500ms
constexpr unsigned long LED_INTERVAL_MS = 100;          // Gap between LEDs (100ms)

// State machine
enum State {
  SEQUENCE,       // Running LED sequence one-by-one
  INTERRUPT       // All LEDs on (button pressed)
};

State currentState = SEQUENCE;
unsigned long stateStartMs = 0;
uint8_t currentLedIndex = 0;  // Which LED is currently on (0-9)
unsigned long ledOffTimeMs = 0;  // When current LED should turn off

// Button state tracking
bool buttonLeftPressed = false;
bool buttonRightPressed = false;
unsigned long lastButtonLeftDebounceMs = 0;
unsigned long lastButtonRightDebounceMs = 0;
bool buttonLeftStable = HIGH;  // With pull-up, unpressed = HIGH
bool buttonRightStable = HIGH;
}  // namespace

void allLedsOff() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_MAP[i], LOW);
  }
}

void allLedsOn() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_MAP[i], HIGH);
  }
}

void updateButtonStates() {
  // Read button states (pull-up: LOW = pressed, HIGH = unpressed)
  bool rawLeftState = digitalRead(BUTTON_LEFT);
  bool rawRightState = digitalRead(BUTTON_RIGHT);
  const unsigned long now = millis();

  // Debounce left button
  if (rawLeftState != buttonLeftStable) {
    // State change detected, start debounce timer
    if (lastButtonLeftDebounceMs == 0) {
      lastButtonLeftDebounceMs = now;
    }
    // Wait for debounce time to pass
    if ((now - lastButtonLeftDebounceMs) >= BUTTON_DEBOUNCE_MS) {
      buttonLeftStable = rawLeftState;
      buttonLeftPressed = (rawLeftState == LOW);  // LOW = pressed
      lastButtonLeftDebounceMs = 0;  // Reset debounce timer
    }
  } else {
    lastButtonLeftDebounceMs = 0;  // Reset if no change
  }

  // Debounce right button
  if (rawRightState != buttonRightStable) {
    // State change detected, start debounce timer
    if (lastButtonRightDebounceMs == 0) {
      lastButtonRightDebounceMs = now;
    }
    // Wait for debounce time to pass
    if ((now - lastButtonRightDebounceMs) >= BUTTON_DEBOUNCE_MS) {
      buttonRightStable = rawRightState;
      buttonRightPressed = (rawRightState == LOW);  // LOW = pressed
      lastButtonRightDebounceMs = 0;  // Reset debounce timer
    }
  } else {
    lastButtonRightDebounceMs = 0;  // Reset if no change
  }
}

void setup() {
  Serial.begin(9600);
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   HARDWARE TEST - LED SEQUENCE & INTERRUPT ║");
  Serial.println("║  Row 1 (POS 1-5):    [1][2][3][4][5]  ║");
  Serial.println("║  Row 2 (POS 6-10):   [6][7][8][9][10] ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  Serial.println("Test Mode: LED Sequence with Button Interrupt");
  Serial.println("  - LEDs light up one-by-one in sequence (500ms each)");
  Serial.println("  - Press LEFT button:  ALL LEDs ON (interrupts sequence)");
  Serial.println("  - Press RIGHT button: ALL LEDs ON (interrupts sequence)");
  Serial.println("  - Release button: Sequence resumes\n");

  Serial.println("Pin Mapping:");
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    Serial.print("  POS-");
    Serial.print(i + 1);
    Serial.print(" -> Arduino Pin ");
    Serial.println(PIN_MAP[i]);
  }
  Serial.println();

  // Initialize LED pins
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    pinMode(PIN_MAP[i], OUTPUT);
  }

  // Initialize buttons with internal pull-up
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  allLedsOff();
  Serial.println("All LEDs initialized.\n");

  // Start sequence - turn on first LED immediately
  currentState = SEQUENCE;
  stateStartMs = millis();
  currentLedIndex = 0;
  digitalWrite(PIN_MAP[currentLedIndex], HIGH);
  ledOffTimeMs = stateStartMs + LED_ON_TIME_MS;

  Serial.print("[");
  Serial.print(stateStartMs);
  Serial.print("ms] LED ");
  Serial.print(currentLedIndex + 1);
  Serial.println(" ON (POS-1) - SEQUENCE START");
}

void loop() {
  // Always update button states
  updateButtonStates();

  const unsigned long now = millis();

  // Debug: Show button states every 500ms
  static unsigned long lastDebugMs = 0;
  if (now - lastDebugMs >= 500) {
    lastDebugMs = now;
    Serial.print("[");
    Serial.print(now);
    Serial.print("ms] DEBUG - Left: ");
    Serial.print(digitalRead(BUTTON_LEFT) == LOW ? "PRESSED" : "released");
    Serial.print(" | Right: ");
    Serial.println(digitalRead(BUTTON_RIGHT) == LOW ? "PRESSED" : "released");
  }

  // Check if either button is pressed
  if (buttonLeftPressed || buttonRightPressed) {
    if (currentState != INTERRUPT) {
      // Transition to INTERRUPT state
      currentState = INTERRUPT;
      stateStartMs = now;
      allLedsOn();

      char button = buttonLeftPressed ? 'L' : 'R';
      Serial.print("[");
      Serial.print(now);
      Serial.print("ms] Button ");
      Serial.print(button);
      Serial.println(" pressed - ALL LEDs ON (INTERRUPT)");
    }
  } else {
    // No button pressed - return to sequence
    if (currentState == INTERRUPT) {
      currentState = SEQUENCE;
      stateStartMs = now;
      currentLedIndex = 0;
      allLedsOff();

      // Turn on first LED immediately
      digitalWrite(PIN_MAP[currentLedIndex], HIGH);
      ledOffTimeMs = now + LED_ON_TIME_MS;

      Serial.print("[");
      Serial.print(now);
      Serial.println("ms] Button released - LED 1 ON (resuming sequence)");
    }
  }

  // Handle SEQUENCE state
  if (currentState == SEQUENCE) {
    // Check if it's time to turn off current LED and move to next
    if (now >= ledOffTimeMs) {
      // Turn off current LED
      if (currentLedIndex < NUM_LEDS) {
        digitalWrite(PIN_MAP[currentLedIndex], LOW);
      }

      // Move to next LED
      currentLedIndex++;
      if (currentLedIndex >= NUM_LEDS) {
        currentLedIndex = 0;  // Loop back to start
        Serial.println(); // Blank line for readability
      }

      // Turn on next LED
      digitalWrite(PIN_MAP[currentLedIndex], HIGH);
      ledOffTimeMs = now + LED_ON_TIME_MS;

      Serial.print("[");
      Serial.print(now);
      Serial.print("ms] LED ");
      Serial.print(currentLedIndex + 1);
      Serial.print(" ON (POS-");
      Serial.print(currentLedIndex + 1);
      Serial.println(")");
    }
  }
}

