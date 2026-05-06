#include "animation.h"

#include <Arduino.h>

#include "peripherals.h"
#include "rgb_panel.h"

namespace animation {

void startupBlink() {
  rgb_panel::setAll(true,  false, false); peripherals::buzzerBeep(1200, 20); delay(120);
  rgb_panel::setAll(false, true,  false); delay(120);
  rgb_panel::setAll(false, false, true ); delay(120);
  rgb_panel::blank();
}

namespace {
constexpr uint32_t kStepMs = 800;
uint32_t g_lastStepMs = 0;
uint8_t  g_phase      = 0;   // 0=R 1=G 2=B 3=off
}  // namespace

void tick() {
  const uint32_t now = millis();
  if (now - g_lastStepMs < kStepMs) return;
  g_lastStepMs = now;
  switch (g_phase) {
    case 0: rgb_panel::setAll(true,  false, false); break;
    case 1: rgb_panel::setAll(false, true,  false); break;
    case 2: rgb_panel::setAll(false, false, true ); break;
    case 3: rgb_panel::blank();                     break;
  }
  g_phase = (uint8_t)((g_phase + 1) & 0x3);
}

}  // namespace animation

