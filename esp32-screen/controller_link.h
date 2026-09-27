// Wireless controller link (ESP-NOW receiver).
//
// Pairs with the esp32-c3-tictactoe-controller firmware: that board
// runs as a remote and broadcasts its button state at ~30 Hz; the
// screen board listens here and exposes the latest mask to the rest of
// the firmware.
//
// Compiled in only when -DSCREEN_REMOTE_CONTROL=1.
#pragma once
#include <Arduino.h>

namespace controller_link {

// Button bits. Matches the sender layout in
// esp32-tictactoe/controller_link.h verbatim.
enum : uint8_t {
  BTN_LEFT   = 1 << 0,
  BTN_RIGHT  = 1 << 1,
  BTN_ROTATE = 1 << 2,  // also = "FIRE" for the invaders build
  BTN_DROP   = 1 << 3,
};

// Bring up ESP-NOW on whichever channel the WiFi STA is currently
// camped on. Safe to call AFTER WiFi has associated (or in AP mode);
// re-initialises if called twice.
void setup();

// Most recent button mask received. Returns 0 when the link is stale
// (no packet for >kStaleMs) so a dropped controller never leaves a
// "stuck" button.
uint8_t buttonMask();

// True if we received a packet within the last kStaleMs.
bool isFresh();

// Milliseconds since the last received packet (UINT32_MAX if none yet).
uint32_t lastRxAgeMs();

}  // namespace controller_link

