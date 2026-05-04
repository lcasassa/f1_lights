// Onboard LED, passive buzzer, and the two front-panel push buttons.
//
// Pin defaults are right for the ESP32-C3 Super Mini; override with
// -DONBOARD_LED_PIN, -DBUZZER_PIN, -DBTN_A_PIN, -DBTN_B_PIN as needed.
// Set BUZZER_PIN=-1 to compile the buzzer code out.
#pragma once
#include <Arduino.h>
#ifndef ONBOARD_LED_PIN
#define ONBOARD_LED_PIN 8
#endif
#ifndef BUZZER_PIN
#define BUZZER_PIN 10
#endif
#ifndef BTN_A_PIN
#define BTN_A_PIN 6
#endif
#ifndef BTN_B_PIN
#define BTN_B_PIN 7
#endif
namespace peripherals {
void setupLed();
void setupBuzzer();
void setupButtons();
// Block-and-beep at \`freq\` Hz for \`ms\` milliseconds.
void buzzerBeep(uint32_t freq, uint32_t ms);
// True while either of the two front-panel buttons is held down.
bool anyButtonPressed();
}  // namespace peripherals
