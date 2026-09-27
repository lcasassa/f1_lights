// Minimal "rgb_panel" stub for the screen build.
//
// The F1 / tic-tac-toe builds drive an HT16K33-backed RGB ring for boot
// + OTA status. The screen build has no such hardware — but we keep the
// same API so wifi_ota.cpp can be shared verbatim across all three ESP32
// builds. All calls here are mapped to the onboard LED on GPIO 8
// (active-low: LOW = on).
#pragma once

#include <Arduino.h>

namespace rgb_panel {

void setLed(uint8_t ledIndex, bool r, bool g, bool b);
void setAll(bool r, bool g, bool b);
void blank();
void setBusy(uint8_t ledIndex, bool on);
void showOtaProgress(int percent);

}  // namespace rgb_panel

