// Wireless controller link (ESP-NOW sender).
//
// Compiled in only when -DTICTACTOE_CONTROLLER=1. Pairs with the
// receiver on the screen board (esp32-screen/controller_link.h). The
// wire format MUST stay byte-identical between the two files.
#pragma once
#include <Arduino.h>

namespace controller_link {

enum : uint8_t {
  BTN_LEFT   = 1 << 0,
  BTN_RIGHT  = 1 << 1,
  BTN_ROTATE = 1 << 2,  // also = "FIRE" for invaders
  BTN_DROP   = 1 << 3,
};

// Bring up WiFi STA (channel only — we don't need an AP) + ESP-NOW and
// register a broadcast peer. Safe to call once at boot.
void setup();

// Send the current button bitmask to the broadcast address. Cheap; OK
// to call every loop, but the controller throttles to ~30 Hz.
void sendButtons(uint8_t mask);

}  // namespace controller_link

