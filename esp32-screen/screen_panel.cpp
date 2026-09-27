#include "screen_panel.h"

#include <Adafruit_NeoPixel.h>

namespace screen_panel {

namespace {

// 2-bit → 8-bit brightness LUT (matches the AVR EXPAND[] table:
// {0, 5, 15, 30}). Keep the panel dim — they're VERY bright at full
// duty, and dim looks better on camera anyway.
constexpr uint8_t kExpand[4] = {0, 5, 15, 30};

Adafruit_NeoPixel g_panelA(LEDS_PER_PANEL, PANEL_A_PIN, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel g_panelB(LEDS_PER_PANEL, PANEL_B_PIN, NEO_GRB + NEO_KHZ800);

uint8_t g_packA[LEDS_PER_PANEL];
uint8_t g_packB[LEDS_PER_PANEL];

}  // namespace

void setup() {
  g_panelA.begin();
  g_panelB.begin();
  clearAll();
  show();
}

void clearAll() {
  memset(g_packA, 0, sizeof(g_packA));
  memset(g_packB, 0, sizeof(g_packB));
}

// Mirrors screen/main.cpp::setPixel exactly so the game logic doesn't
// need to know which physical panel a column lives on.
void setPixel(uint8_t x, uint8_t y, uint8_t packed) {
  if (x >= MATRIX_W || y >= MATRIX_H) return;
  if (x < PANEL_W) {
    // Panel A — even rows R→L, odd rows L→R
    uint16_t idx;
    if (y & 1) idx = y * PANEL_W + x;
    else       idx = y * PANEL_W + (PANEL_W - 1 - x);
    g_packA[idx] = packed;
  } else {
    // Panel B — flipped vertically + columns mirrored
    uint8_t px = PANEL_W - 1 - (x - PANEL_W);
    uint8_t py = PANEL_H - 1 - y;
    uint16_t idx;
    if (py & 1) idx = py * PANEL_W + px;
    else        idx = py * PANEL_W + (PANEL_W - 1 - px);
    g_packB[idx] = packed;
  }
}

static inline void expandInto(Adafruit_NeoPixel &strip, const uint8_t *pack) {
  for (uint16_t i = 0; i < LEDS_PER_PANEL; i++) {
    uint8_t p = pack[i];
    uint8_t r = kExpand[(p >> 6) & 0x03];
    uint8_t g = kExpand[(p >> 4) & 0x03];
    uint8_t b = kExpand[(p >> 2) & 0x03];
    strip.setPixelColor(i, r, g, b);
  }
}

void show() {
  expandInto(g_panelA, g_packA);
  expandInto(g_panelB, g_packB);
  g_panelA.show();
  g_panelB.show();
}

}  // namespace screen_panel

