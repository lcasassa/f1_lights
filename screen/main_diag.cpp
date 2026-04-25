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

  Serial.println(F("Walking Screen A..."));
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    ws2812_onePixel(LED_PIN_A, i, NUM_LEDS, BRIGHTNESS, BRIGHTNESS, BRIGHTNESS);
    ws2812_black(LED_PIN_B, NUM_LEDS);
    if (i % 8 == 0) {
      Serial.print(F("  A row ")); Serial.print(i / 8);
      Serial.print(F("  (LED ")); Serial.print(i); Serial.println(F(")"));
    }
    delay(10);
  }

  Serial.println(F("Walking Screen B..."));
  for (uint16_t i = 0; i < NUM_LEDS; i++) {
    ws2812_black(LED_PIN_A, NUM_LEDS);
    ws2812_onePixel(LED_PIN_B, i, NUM_LEDS, BRIGHTNESS, BRIGHTNESS, BRIGHTNESS);
    if (i % 8 == 0) {
      Serial.print(F("  B row ")); Serial.print(i / 8);
      Serial.print(F("  (LED ")); Serial.print(i); Serial.println(F(")"));
    }
    delay(10);
  }

  ws2812_black(LED_PIN_A, NUM_LEDS);
  ws2812_black(LED_PIN_B, NUM_LEDS);
  delay(250);

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

void loop() {
  static bool on = false;
  on = !on;
  if (on) {
    ws2812_onePixel(LED_PIN_A, 0, NUM_LEDS, BRIGHTNESS, 0, 0);
    ws2812_onePixel(LED_PIN_B, 0, NUM_LEDS, 0, 0, BRIGHTNESS);
  } else {
    ws2812_black(LED_PIN_A, NUM_LEDS);
    ws2812_black(LED_PIN_B, NUM_LEDS);
  }
  delay(500);
}
