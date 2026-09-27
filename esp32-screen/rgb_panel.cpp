#include "rgb_panel.h"

#include "peripherals.h"   // for ONBOARD_LED_PIN

namespace rgb_panel {

namespace {
// Onboard LED on the ESP32-C3 Super Mini is active-LOW.
inline void ledOn()  { digitalWrite(ONBOARD_LED_PIN, LOW);  }
inline void ledOff() { digitalWrite(ONBOARD_LED_PIN, HIGH); }
}  // namespace

void setLed(uint8_t /*ledIndex*/, bool r, bool g, bool b) {
  if (r || g || b) ledOn(); else ledOff();
}

void setAll(bool r, bool g, bool b) {
  if (r || g || b) ledOn(); else ledOff();
}

void blank() { ledOff(); }

void setBusy(uint8_t /*ledIndex*/, bool on) {
  if (on) ledOn(); else ledOff();
}

void showOtaProgress(int percent) {
  // Coarse heartbeat: LED on while OTA in progress, off at 100%.
  if (percent >= 100) ledOff(); else ledOn();
}

}  // namespace rgb_panel

