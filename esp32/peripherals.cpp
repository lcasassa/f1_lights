#include "peripherals.h"

namespace peripherals {

void setupLed() {
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(ONBOARD_LED_PIN, HIGH);  // off (active-low)
}

void setupBuzzer() {
#if BUZZER_PIN >= 0
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
#endif
}

void setupButtons() {
  pinMode(BTN_A_PIN, INPUT_PULLUP);
  pinMode(BTN_B_PIN, INPUT_PULLUP);
}

void buzzerBeep(uint32_t freq, uint32_t ms) {
#if BUZZER_PIN >= 0
  tone(BUZZER_PIN, freq, ms);
  delay(ms);
  noTone(BUZZER_PIN);
  digitalWrite(BUZZER_PIN, LOW);
#else
  (void)freq; (void)ms;
#endif
}

bool anyButtonPressed() {
  return (digitalRead(BTN_A_PIN) == LOW) || (digitalRead(BTN_B_PIN) == LOW);
}

}  // namespace peripherals

