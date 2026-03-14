#include <Arduino.h>

namespace {
// F1 Start Light Configuration
// Logical LED positions: 1-5 (top row), 6-10 (bottom row)
// Map each logical position to the actual Arduino pin it's connected to
//
// Example layout:
//   Physical Row 1: [POS-1] [POS-2] [POS-3] [POS-4] [POS-5]
//   Physical Row 2: [POS-6] [POS-7] [POS-8] [POS-9] [POS-10]
//
// Adjust the pins below to match your actual wiring:
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
constexpr uint8_t BUTTON_LEFT = 2;                      // Left button on pin 2 (pull-up)
constexpr uint8_t BUTTON_RIGHT = 3;                     // Right button on pin 3 (pull-up)
constexpr unsigned long BUTTON_DEBOUNCE_MS = 50;        // Debounce time in milliseconds

// F1 Start Sequence Timing (correct F1 specifications)
constexpr unsigned long LIGHT_INTERVAL_MS = 1000;       // 1 second (1000ms) between each light turning on
constexpr unsigned long MIN_RANDOM_DELAY_MS = 1000;     // Min random delay after all lights on
constexpr unsigned long MAX_RANDOM_DELAY_MS = 3000;     // Max random delay after all lights on
constexpr unsigned long RESTART_DELAY_MS = 3000;        // Delay before next sequence starts

// State machine
enum State {
  IDLE,           // Waiting to start sequence
  LIGHTING_UP,    // Lights turning on left-to-right (both rows simultaneously, 100ms intervals)
  ALL_ON,         // All lights on for 500ms before GO
  BLACKOUT,       // All lights off (START!)
  RESTART_WAIT    // Waiting before next sequence
};

State currentState = IDLE;
unsigned long stateStartMs = 0;
uint8_t litColumnCount = 0;  // How many columns are currently lit (0-5)
unsigned long randomGoDelay = 0;  // Random delay before GO signal

// Button state tracking
bool buttonLeftPressed = false;
bool buttonRightPressed = false;
bool buttonLeftRaw = HIGH;
bool buttonRightRaw = HIGH;
unsigned long lastButtonCheckMs = 0;
}  // namespace

void allLedsOff() {
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    digitalWrite(PIN_MAP[i], LOW);
  }
}

void updateButtonStates() {
  const unsigned long now = millis();

  // Only check buttons every BUTTON_DEBOUNCE_MS milliseconds
  if (now - lastButtonCheckMs < BUTTON_DEBOUNCE_MS) {
    return;
  }
  lastButtonCheckMs = now;

  // Read current button states (LOW = pressed due to pull-up)
  bool leftRaw = digitalRead(BUTTON_LEFT);
  bool rightRaw = digitalRead(BUTTON_RIGHT);

  // Update pressed states
  buttonLeftPressed = (leftRaw == LOW);
  buttonRightPressed = (rightRaw == LOW);
}

void setup() {
  Serial.begin(9600);
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║     F1 START LIGHTS SEQUENCE (10 LED)  ║");
  Serial.println("║  Row 1 (POS 1-5):    [1][2][3][4][5]  ║");
  Serial.println("║  Row 2 (POS 6-10):   [6][7][8][9][10] ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  Serial.println("Button Configuration:");
  Serial.println("  Pin 2 (Left Button):  Pull-up enabled");
  Serial.println("  Pin 3 (Right Button): Pull-up enabled");
  Serial.println("  Press BOTH buttons simultaneously to start sequence\n");

  Serial.println("Pin Mapping:");
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    Serial.print("  POS-");
    Serial.print(i + 1);
    Serial.print(" -> Arduino Pin ");
    Serial.println(PIN_MAP[i]);
  }
  Serial.println();

  Serial.println("Sequence (F1 Standard):");
  Serial.println("  1. Five lights illuminate left-to-right (1 second intervals, both rows)");
  Serial.println("  2. All lights on for random delay (1-3 seconds)");
  Serial.println("  3. ALL LIGHTS OFF = GO SIGNAL!");
  Serial.println("  4. Sequence repeats after 3 seconds\n");

  // Initialize LED pins
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    pinMode(PIN_MAP[i], OUTPUT);
  }

  // Initialize buttons with internal pull-up
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  allLedsOff();
  Serial.println("All LEDs initialized.");
  Serial.println("Waiting for button press...\n");

  // Seed random number generator
  randomSeed(analogRead(A0));

  // Start in IDLE state, waiting for button press
  currentState = IDLE;
  stateStartMs = millis();
}

void loop() {
  // Always update button states
  updateButtonStates();

  const unsigned long now = millis();
  const unsigned long elapsedInState = now - stateStartMs;

  switch (currentState) {
    case IDLE: {
      // Wait for both buttons to be pressed
      if (buttonLeftPressed && buttonRightPressed) {
        currentState = LIGHTING_UP;
        stateStartMs = now;
        litColumnCount = 0;
        Serial.print("[");
        Serial.print(now);
        Serial.println("ms] ═══ BOTH BUTTONS PRESSED - SEQUENCE START ═══");
      }
      break;
    }

    // ...existing code...
    case LIGHTING_UP: {
      // Turn on next column (both rows simultaneously) at interval
      if (elapsedInState >= litColumnCount * LIGHT_INTERVAL_MS && litColumnCount < 5) {
        // Light up column: POS-(litColumnCount+1) and POS-(litColumnCount+6)
        // Column 0: POS-1 (pin 0) and POS-6 (pin 5)
        // Column 1: POS-2 (pin 1) and POS-7 (pin 6)
        // ... etc
        digitalWrite(PIN_MAP[litColumnCount], HIGH);           // Top row
        digitalWrite(PIN_MAP[litColumnCount + 5], HIGH);       // Bottom row

        Serial.print("[");
        Serial.print(now);
        Serial.print("ms] Column ");
        Serial.print(litColumnCount + 1);
        Serial.print(" ON (POS-");
        Serial.print(litColumnCount + 1);
        Serial.print(" + POS-");
        Serial.print(litColumnCount + 6);
        Serial.println(")");

        litColumnCount++;
      }

      // Transition to ALL_ON state when all columns are lit
      if (litColumnCount >= 5 && elapsedInState >= 5 * LIGHT_INTERVAL_MS) {
        currentState = ALL_ON;
        stateStartMs = now;
        randomGoDelay = random(MIN_RANDOM_DELAY_MS, MAX_RANDOM_DELAY_MS + 1);

        Serial.print("[");
        Serial.print(now);
        Serial.print("ms] All LEDs ON - random delay before GO: ");
        Serial.print(randomGoDelay);
        Serial.println("ms");
      }
      break;
    }

    case ALL_ON: {
      // Hold all lights on for random delay (F1 standard)
      if (elapsedInState >= randomGoDelay) {
        currentState = BLACKOUT;
        stateStartMs = now;
        allLedsOff();
        Serial.print("[");
        Serial.print(now);
        Serial.println("ms] *** LIGHTS OUT - GO! ***");
      }
      break;
    }

    case BLACKOUT: {
      // Keep lights off for 500ms
      if (elapsedInState >= 500) {
        currentState = RESTART_WAIT;
        stateStartMs = now;
        Serial.print("[");
        Serial.print(now);
        Serial.println("ms] Waiting to restart sequence...");
      }
      break;
    }

    case RESTART_WAIT: {
      // Wait before returning to IDLE state for next button press
      if (elapsedInState >= RESTART_DELAY_MS) {
        currentState = IDLE;
        stateStartMs = now;
        Serial.print("[");
        Serial.print(now);
        Serial.println("ms] Sequence complete - waiting for button press...");
      }
      break;
    }

    default:
      break;
  }
}

