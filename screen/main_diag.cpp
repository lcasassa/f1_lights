#include <Arduino.h>
#include "ws2812.h"

#define LED_PIN_A   6
#define LED_PIN_B   7
#define NUM_LEDS    256
#define BRIGHTNESS  15

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  ws2812_black(LED_PIN_A, NUM_LEDS);
  ws2812_black(LED_PIN_B, NUM_LEDS);
  delay(250);

  Serial.println(F("\n=== LED Chain Diagnostic (2 screens) ==="));

  Serial.println(F("All red..."));
  ws2812_solid(LED_PIN_A, 0, BRIGHTNESS, 0, NUM_LEDS);
  ws2812_solid(LED_PIN_B, 0, BRIGHTNESS, 0, NUM_LEDS);
  delay(500);

  Serial.println(F("All green..."));
  ws2812_solid(LED_PIN_A, BRIGHTNESS, 0, 0, NUM_LEDS);
  ws2812_solid(LED_PIN_B, BRIGHTNESS, 0, 0, NUM_LEDS);
  delay(500);

  Serial.println(F("All blue..."));
  ws2812_solid(LED_PIN_A, 0, 0, BRIGHTNESS, NUM_LEDS);
  ws2812_solid(LED_PIN_B, 0, 0, BRIGHTNESS, NUM_LEDS);
  delay(500);

  Serial.println(F("Screen A (pin 6) = RED,  Screen B (pin 7) = BLUE"));
  ws2812_solid(LED_PIN_A, 0, BRIGHTNESS, 0, NUM_LEDS);
  ws2812_solid(LED_PIN_B, 0, 0, BRIGHTNESS, NUM_LEDS);
  delay(2000);

  Serial.println(F("\n=== Diagnostic complete ==="));
}

// HSV to RGB (h 0-255, returns scaled by BRIGHTNESS)
static void hsvToRgb(uint8_t h, uint8_t &r, uint8_t &g, uint8_t &b) {
  uint8_t region = h / 43;
  uint8_t rem = (h % 43) * 6;
  switch (region) {
    case 0: r=255; g=rem;   b=0;     break;
    case 1: r=255-rem; g=255; b=0;   break;
    case 2: r=0;   g=255; b=rem;     break;
    case 3: r=0;   g=255-rem; b=255; break;
    case 4: r=rem; g=0;   b=255;     break;
    default:r=255; g=0;   b=255-rem; break;
  }
  r = (uint16_t)r * BRIGHTNESS / 255;
  g = (uint16_t)g * BRIGHTNESS / 255;
  b = (uint16_t)b * BRIGHTNESS / 255;
}

// Temp buffer for one strip (3 bytes per LED — fits in RAM for 256 LEDs? No, 768 bytes each.)
// Send pixel-by-pixel instead to save RAM.
static void rainbowStrip(uint8_t pin, uint8_t offset) {
  volatile uint8_t *port = portOutputRegister(digitalPinToPort(pin));
  uint8_t pinMask = digitalPinToBitMask(pin);
  uint8_t hi = *port |  pinMask;
  uint8_t lo = *port & ~pinMask;
  cli();
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    uint8_t h = offset + (uint16_t)i * 256 / NUM_LEDS;
    uint8_t r, g, b;
    hsvToRgb(h, r, g, b);
    ws2812_sendByte(port, hi, lo, g);
    ws2812_sendByte(port, hi, lo, r);
    ws2812_sendByte(port, hi, lo, b);
  }
  sei();
  delayMicroseconds(80);
}

void loop() {
  static uint8_t offset = 0;
  rainbowStrip(LED_PIN_A, offset);
  rainbowStrip(LED_PIN_B, offset + 128);  // opposite hue on panel B
  offset += 2;
  delay(30);
}
