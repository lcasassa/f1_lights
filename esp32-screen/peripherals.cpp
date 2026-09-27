#include "peripherals.h"

namespace peripherals {

void setupLed() {
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(ONBOARD_LED_PIN, HIGH);  // off (active-low)
}

void setupButtons() {
  pinMode(BTN_LEFT_PIN,   INPUT_PULLUP);
  pinMode(BTN_RIGHT_PIN,  INPUT_PULLUP);
  pinMode(BTN_ROTATE_PIN, INPUT_PULLUP);
  pinMode(BTN_DROP_PIN,   INPUT_PULLUP);
}

bool buttonLeft()   { return digitalRead(BTN_LEFT_PIN)   == LOW; }
bool buttonRight()  { return digitalRead(BTN_RIGHT_PIN)  == LOW; }
bool buttonRotate() { return digitalRead(BTN_ROTATE_PIN) == LOW; }
bool buttonDrop()   { return digitalRead(BTN_DROP_PIN)   == LOW; }

bool bothSideButtonsPressed() {
  return buttonLeft() && buttonRight();
}

bool anyButtonPressed() {
  return buttonLeft() || buttonRight() || buttonRotate() || buttonDrop();
}

}  // namespace peripherals

