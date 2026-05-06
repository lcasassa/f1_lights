// Onboard LED + (optional) passive buzzer for the tic-tac-toe board.
//
// The tic-tac-toe variant has no front-panel buttons (the F1 build's
// BTN_A/BTN_B inputs were removed), so this header only exposes the
// onboard LED and a buzzer helper. Override pins via -DONBOARD_LED_PIN
// / -DBUZZER_PIN; set BUZZER_PIN=-1 to compile the buzzer out entirely.
#pragma once
#include <Arduino.h>

#ifndef ONBOARD_LED_PIN
#define ONBOARD_LED_PIN 8
#endif
#ifndef BUZZER_PIN
#define BUZZER_PIN -1
#endif

namespace peripherals {

void setupLed();
void setupBuzzer();

// Block-and-beep at `freq` Hz for `ms` milliseconds. No-op when
// BUZZER_PIN < 0.
void buzzerBeep(uint32_t freq, uint32_t ms);

}  // namespace peripherals

