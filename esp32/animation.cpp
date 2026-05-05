#include "animation.h"

#include <Arduino.h>

#include "rgb_panel.h"
#include "seg7.h"

namespace animation {

void startupBlink() {
  // Issue the seg7 writes first so the last I²C transaction on dig 0/1 is
  // the RGB-only word; the shared HT16K33 shadow keeps them disjoint.
  seg7::writeText(seg7::kTop, "8.8.8.8.8.");
  seg7::writeText(seg7::kBot, "8.8.8.8.8.");
  rgb_panel::setAll(true, false, false);
  delay(50);

  rgb_panel::blank();
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
}

namespace {

// Random increment in [minInc, 0.1] where minInc grows with the integer
// part so each tick is at least one visible LSB on the 5-digit panel.
float randomIncrement(float currentValue) {
  uint32_t v = (uint32_t)currentValue;
  float minInc;
  if      (v >= 10000) minInc = 1.0f;
  else if (v >= 1000)  minInc = 0.1f;
  else if (v >= 100)   minInc = 0.01f;
  else if (v >= 10)    minInc = 0.001f;
  else                 minInc = 0.0001f;
  const float maxInc = 0.1f;
  if (minInc > maxInc) minInc = maxInc;
  float t = (float)random(0, 10001) / 10000.0f;  // 0..1
  return minInc + t * (maxInc - minInc);
}

bool     g_wasRunning  = false;
uint32_t g_lastTick    = 0;
float    g_segCounter  = 0;

}  // namespace

void tick(bool running) {
  if (!running) {
    if (g_wasRunning) {
      seg7::clear(seg7::kTop);
      seg7::clear(seg7::kBot);
      rgb_panel::blank();
      g_wasRunning = false;
    }
    return;
  }
  g_wasRunning = true;

  uint32_t now = millis();
  if (now - g_lastTick < 2500) return;
  g_lastTick = now;

  rgb_panel::tickRandomRed();
  seg7::printFloat(seg7::kTop, g_segCounter);
  seg7::printFloat(seg7::kBot, g_segCounter);
  g_segCounter += randomIncrement(g_segCounter);
}

}  // namespace animation

