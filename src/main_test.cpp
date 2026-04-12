#include <Arduino.h>

namespace {
// F1 Start Light Configuration
// Logical LED positions: 1-5 (top row), 6-10 (bottom row)
// Map each logical position to the actual Arduino pin it's connected to
constexpr uint8_t PIN_MAP[] = {
  4,   // POS-1  (Row 1, leftmost)
  11,  // POS-2  (Row 1)
  12,  // POS-3  (Row 1)
  A0,  // POS-4  (Row 1)
  A1,  // POS-5  (Row 1, rightmost)
  5,   // POS-6  (Row 2, leftmost)
  10,  // POS-7  (Row 2)
  9,   // POS-8  (Row 2)
  8,   // POS-9  (Row 2)
  7,    // POS-10 (Row 2, rightmost)

  13    // POS-11 (extra LED for testing)
};

constexpr uint8_t NUM_LEDS = 11;

// Buzzer Configuration (Grove Passive Buzzer v1.1 on PWM pin 6)
constexpr uint8_t BUZZER_PIN = 6;

// Button Configuration
constexpr uint8_t BUTTON_LEFT = 2;                      // Left button on pin 2 (pull-up)
constexpr uint8_t BUTTON_RIGHT = 3;                     // Right button on pin 3 (pull-up)
constexpr unsigned long BUTTON_DEBOUNCE_MS = 10;        // Debounce time in milliseconds

// Hardware Test Sequence
constexpr unsigned long LED_ON_TIME_MS = 500;           // Each LED on for 500ms

// State machine
enum State {
  SEQUENCE,       // Running LED sequence one-by-one
  PLAYING_SONG    // Playing a melody (blocking)
};

State currentState = SEQUENCE;
unsigned long stateStartMs = 0;
uint8_t currentLedIndex = 0;
unsigned long ledOffTimeMs = 0;

// Button state tracking (edge detection)
bool buttonLeftPressed = false;
bool buttonRightPressed = false;
bool prevButtonLeftPressed = false;
bool prevButtonRightPressed = false;
unsigned long lastButtonLeftDebounceMs = 0;
unsigned long lastButtonRightDebounceMs = 0;
bool buttonLeftStable = HIGH;
bool buttonRightStable = HIGH;

// ── Song selection ──────────────────────────────────────────────────────────
int8_t currentSongIndex = 0;

// ── Buzzer helpers ──────────────────────────────────────────────────────────

// Helper: play a note then silence gap (blocking).
static void n(unsigned int freq, unsigned long durMs, unsigned long gapMs = 20) {
  tone(BUZZER_PIN, freq, durMs);
  delay(durMs + gapMs);
}

// Helper: rest (silence)
static void r(unsigned long durMs) {
  noTone(BUZZER_PIN);
  delay(durMs);
}

// ── Winner melodies — 10 iconic tunes everyone can recognise ────────────────
// Note frequencies (Hz):
//   C4=262  C#4=277  D4=294  Eb4=311  E4=330  F4=349  F#4=370
//   G4=392  G#4=415  A4=440  Bb4=466  B4=494
//   C5=523  C#5=554  D5=587  Eb5=622  E5=659  F5=698  F#5=740
//   G5=784  G#5=831  A5=880  Bb5=932  B5=988
//   C6=1047 D6=1175  E6=1319 F6=1397  G6=1568

// 1) Super Mario Bros — main theme opening (~200 BPM)
static void winnerMelody1() {
  constexpr int E = 150, Q = 300, DQ = 450;
  n(659, E); n(659, E); r(E);
  n(659, E); r(E); n(523, E);
  n(659, Q); r(E);
  n(784, DQ); r(E); r(Q);
  n(392, DQ);
  noTone(BUZZER_PIN);
}

// 2) Imperial March — Star Wars (~108 BPM)
static void winnerMelody2() {
  constexpr int Q = 500, DQ = 750, E = 250;
  n(392, DQ); n(392, DQ); n(392, DQ);
  n(311, Q);  n(466, E);
  n(392, DQ);
  n(311, Q);  n(466, E);
  n(392, DQ + Q);
  noTone(BUZZER_PIN);
}

// 3) Smoke On The Water — Deep Purple (~112 BPM)
static void winnerMelody3() {
  constexpr int E = 268, Q = 535, DQ = 803;
  n(392, E); n(466, E); n(523, DQ); r(E);
  n(392, E); n(466, E); n(554, E); n(523, DQ); r(E);
  n(392, E); n(466, E); n(523, Q); n(466, E); n(392, DQ);
  noTone(BUZZER_PIN);
}

// 4) Ode To Joy — Beethoven's 9th (~120 BPM)
static void winnerMelody4() {
  constexpr int Q = 400, DQ = 600, H = 800, E = 200;
  n(330, Q); n(330, Q); n(349, Q); n(392, Q);
  n(392, Q); n(349, Q); n(330, Q); n(294, Q);
  n(262, Q); n(262, Q); n(294, Q); n(330, Q);
  n(330, DQ); n(294, E); n(294, H);
  noTone(BUZZER_PIN);
}

// 5) We Are The Champions — Queen (~62 BPM)
static void winnerMelody5() {
  n(349, 250); n(415, 250); n(523, 250); n(587, 500);
  n(523, 200); n(466, 200); n(440, 350); n(466, 600);
  r(100);
  n(466, 250); n(523, 250); n(587, 800);
  noTone(BUZZER_PIN);
}

// 6) We Will Rock You — Queen (~82 BPM)
static void winnerMelody6() {
  n(147, 150); n(147, 150); r(100); n(1200, 80); r(350);
  n(147, 150); n(147, 150); r(100); n(1200, 80); r(350);
  n(440, 180); n(440, 180); n(440, 180); n(440, 180);
  n(440, 250); n(392, 600);
  noTone(BUZZER_PIN);
}

// 7) Jingle Bells — chorus (~120 BPM)
static void winnerMelody7() {
  constexpr int E = 180, Q = 360, H = 720;
  n(659, E); n(659, E); n(659, Q); r(40);
  n(659, E); n(659, E); n(659, Q); r(40);
  n(659, E); n(784, E); n(523, E); n(587, E);
  n(659, H);
  noTone(BUZZER_PIN);
}

// 8) Seven Nation Army — The White Stripes (~124 BPM)
static void winnerMelody8() {
  constexpr int E = 242, Q = 484, DQ = 726, H = 968;
  n(330, Q + E); r(20);
  n(330, E); n(392, Q); r(20);
  n(330, E); r(20);
  n(294, DQ); r(20);
  n(262, DQ); r(20);
  n(247, H);
  noTone(BUZZER_PIN);
}

// 9) The Final Countdown — Europe (~118 BPM)
static void winnerMelody9() {
  constexpr int S = 127, E = 254, Q = 508, DQ = 762;
  n(554, S); n(587, Q); r(S); n(554, E); r(S); n(440, DQ); r(E);
  n(554, S); n(587, Q); r(S); n(554, E); n(440, Q); r(E);
  n(659, S); n(740, Q); r(S); n(659, E); r(S); n(554, E);
  r(S); n(587, E); n(554, Q);
  noTone(BUZZER_PIN);
}

// 10) Axel F — Beverly Hills Cop (~105 BPM, triplet swing)
static void winnerMelody10() {
  constexpr int T = 190, L = 380, S = 190, Q = 571;
  n(349, L);  r(S);
  n(415, L);  r(S);
  n(349, S);  n(349, S);
  n(466, L);  n(349, S);
  n(311, L);
  r(T);
  n(349, L);  r(S);
  n(523, L);  r(S);
  n(349, S);  n(349, S);
  n(554, L);  n(523, S);
  n(415, S);
  n(349, S);  n(523, S);
  n(698, Q);
  noTone(BUZZER_PIN);
}

// Song names for Serial output
const char* const SONG_NAMES[] = {
  "1) Super Mario Bros",
  "2) Imperial March (Star Wars)",
  "3) Smoke On The Water (Deep Purple)",
  "4) Ode To Joy (Beethoven)",
  "5) We Are The Champions (Queen)",
  "6) We Will Rock You (Queen)",
  "7) Jingle Bells",
  "8) Seven Nation Army (White Stripes)",
  "9) The Final Countdown (Europe)",
  "10) Axel F (Beverly Hills Cop)"
};

typedef void (*MelodyFn)();
constexpr uint8_t NUM_SONGS = 10;
MelodyFn songs[NUM_SONGS] = {
  winnerMelody1, winnerMelody2, winnerMelody3, winnerMelody4, winnerMelody5,
  winnerMelody6, winnerMelody7, winnerMelody8, winnerMelody9, winnerMelody10
};

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
  bool rawLeftState = digitalRead(BUTTON_LEFT);
  bool rawRightState = digitalRead(BUTTON_RIGHT);
  const unsigned long now = millis();

  // Debounce left button
  if (rawLeftState != buttonLeftStable) {
    if (lastButtonLeftDebounceMs == 0) {
      lastButtonLeftDebounceMs = now;
    }
    if ((now - lastButtonLeftDebounceMs) >= BUTTON_DEBOUNCE_MS) {
      buttonLeftStable = rawLeftState;
      buttonLeftPressed = (rawLeftState == LOW);
      lastButtonLeftDebounceMs = 0;
    }
  } else {
    lastButtonLeftDebounceMs = 0;
  }

  // Debounce right button
  if (rawRightState != buttonRightStable) {
    if (lastButtonRightDebounceMs == 0) {
      lastButtonRightDebounceMs = now;
    }
    if ((now - lastButtonRightDebounceMs) >= BUTTON_DEBOUNCE_MS) {
      buttonRightStable = rawRightState;
      buttonRightPressed = (rawRightState == LOW);
      lastButtonRightDebounceMs = 0;
    }
  } else {
    lastButtonRightDebounceMs = 0;
  }
}

void printCurrentSong() {
  Serial.print("  >> Now selected: ");
  Serial.println(SONG_NAMES[currentSongIndex]);
}

void playSong() {
  Serial.print("[");
  Serial.print(millis());
  Serial.print("ms] Playing: ");
  Serial.println(SONG_NAMES[currentSongIndex]);

  allLedsOn();
  songs[currentSongIndex]();
  allLedsOff();

  Serial.print("[");
  Serial.print(millis());
  Serial.println("ms] Done.");
}

void setup() {
  Serial.begin(9600);
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   SONG TESTER - 10 Winner Melodies     ║");
  Serial.println("╚════════════════════════════════════════╝\n");

  Serial.println("Controls:");
  Serial.println("  LEFT  button (pin 2): Previous song & play");
  Serial.println("  RIGHT button (pin 3): Next song & play\n");

  Serial.println("Songs:");
  for (uint8_t i = 0; i < NUM_SONGS; i++) {
    Serial.print("  ");
    Serial.println(SONG_NAMES[i]);
  }
  Serial.println();

  // Initialize LED pins
  for (uint8_t i = 0; i < NUM_LEDS; i++) {
    pinMode(PIN_MAP[i], OUTPUT);
  }

  // Initialize buttons with internal pull-up
  pinMode(BUTTON_LEFT, INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);

  // Initialize buzzer pin
  pinMode(BUZZER_PIN, OUTPUT);

  allLedsOff();
  Serial.println("Ready! Press a button to play a song.\n");

  printCurrentSong();

  // Start LED sequence
  currentState = SEQUENCE;
  stateStartMs = millis();
  currentLedIndex = 0;
  digitalWrite(PIN_MAP[currentLedIndex], HIGH);
  ledOffTimeMs = stateStartMs + LED_ON_TIME_MS;
}

void loop() {
  updateButtonStates();

  const unsigned long now = millis();

  // Edge detection: trigger on press (LOW), not hold
  bool leftJustPressed = (buttonLeftPressed && !prevButtonLeftPressed);
  bool rightJustPressed = (buttonRightPressed && !prevButtonRightPressed);
  prevButtonLeftPressed = buttonLeftPressed;
  prevButtonRightPressed = buttonRightPressed;

  if (leftJustPressed) {
    // Previous song (wrap around)
    currentSongIndex--;
    if (currentSongIndex < 0) currentSongIndex = NUM_SONGS - 1;
    printCurrentSong();
    playSong();

    // After blocking melody, reset LED sequence
    currentLedIndex = 0;
    digitalWrite(PIN_MAP[currentLedIndex], HIGH);
    ledOffTimeMs = millis() + LED_ON_TIME_MS;
    return;
  }

  if (rightJustPressed) {
    // Next song (wrap around)
    currentSongIndex++;
    if (currentSongIndex >= NUM_SONGS) currentSongIndex = 0;
    printCurrentSong();
    playSong();

    // After blocking melody, reset LED sequence
    currentLedIndex = 0;
    digitalWrite(PIN_MAP[currentLedIndex], HIGH);
    ledOffTimeMs = millis() + LED_ON_TIME_MS;
    return;
  }

  // Handle LED sequence (background animation)
  if (now >= ledOffTimeMs) {
    if (currentLedIndex < NUM_LEDS) {
      digitalWrite(PIN_MAP[currentLedIndex], LOW);
    }

    currentLedIndex++;
    if (currentLedIndex >= NUM_LEDS) {
      currentLedIndex = 0;
    }

    digitalWrite(PIN_MAP[currentLedIndex], HIGH);
    ledOffTimeMs = now + LED_ON_TIME_MS;
  }
}

