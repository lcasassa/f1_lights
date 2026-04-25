#pragma once
#include <Arduino.h>
#include <avr/interrupt.h>

// --- WS2812B inline-asm driver for 16 MHz AVR ---

static inline void ws2812_sendByte(volatile uint8_t *port,
                                   uint8_t hi, uint8_t lo, uint8_t b) {
  asm volatile (
    "    ldi  r17, 8          \n\t"
    "loop_%=:                 \n\t"
    "    st   X, %[hi]        \n\t"
    "    sbrs %[byte], 7      \n\t"
    "    st   X, %[lo]        \n\t"
    "    nop \n\t nop \n\t nop \n\t nop \n\t nop \n\t"
    "    sbrc %[byte], 7      \n\t"
    "    st   X, %[lo]        \n\t"
    "    lsl  %[byte]         \n\t"
    "    nop \n\t nop \n\t nop \n\t"
    "    dec  r17             \n\t"
    "    brne loop_%=         \n\t"
    : [byte] "+r" (b)
    : [hi] "r" (hi), [lo] "r" (lo), "x" (port)
    : "r17"
  );
}

// Send N pixels of the same GRB color
static void ws2812_solid(uint8_t pin, uint8_t g, uint8_t r, uint8_t b, uint16_t count) {
  volatile uint8_t *port = portOutputRegister(digitalPinToPort(pin));
  uint8_t pinMask = digitalPinToBitMask(pin);
  uint8_t hi = *port |  pinMask;
  uint8_t lo = *port & ~pinMask;

  cli();
  for (uint16_t i = 0; i < count; i++) {
    ws2812_sendByte(port, hi, lo, g);
    ws2812_sendByte(port, hi, lo, r);
    ws2812_sendByte(port, hi, lo, b);
  }
  sei();
  delayMicroseconds(80);
}

// Send all black
static inline void ws2812_black(uint8_t pin, uint16_t count) {
  ws2812_solid(pin, 0, 0, 0, count);
}

// Light one pixel at idx, rest black
static void ws2812_onePixel(uint8_t pin, uint16_t idx, uint16_t total,
                            uint8_t g, uint8_t r, uint8_t b) {
  volatile uint8_t *port = portOutputRegister(digitalPinToPort(pin));
  uint8_t pinMask = digitalPinToBitMask(pin);
  uint8_t hi = *port |  pinMask;
  uint8_t lo = *port & ~pinMask;

  cli();
  for (uint16_t i = 0; i < idx; i++) {
    ws2812_sendByte(port, hi, lo, 0);
    ws2812_sendByte(port, hi, lo, 0);
    ws2812_sendByte(port, hi, lo, 0);
  }
  ws2812_sendByte(port, hi, lo, g);
  ws2812_sendByte(port, hi, lo, r);
  ws2812_sendByte(port, hi, lo, b);
  for (uint16_t i = idx + 1; i < total; i++) {
    ws2812_sendByte(port, hi, lo, 0);
    ws2812_sendByte(port, hi, lo, 0);
    ws2812_sendByte(port, hi, lo, 0);
  }
  sei();
  delayMicroseconds(80);
}

// Send compact pixel buffer (1 byte per pixel, [RR GG BB xx], 2 bits each).
// expand[] maps 2-bit values to 8-bit output levels.
static void ws2812_showCompact(uint8_t pin, const uint8_t *buf, uint16_t count,
                               const uint8_t expand[4]) {
  volatile uint8_t *port = portOutputRegister(digitalPinToPort(pin));
  uint8_t pinMask = digitalPinToBitMask(pin);
  uint8_t hi = *port |  pinMask;
  uint8_t lo = *port & ~pinMask;

  cli();
  for (uint16_t i = 0; i < count; i++) {
    uint8_t p = buf[i];
    ws2812_sendByte(port, hi, lo, expand[(p >> 4) & 0x03]);  // G
    ws2812_sendByte(port, hi, lo, expand[(p >> 6) & 0x03]);  // R
    ws2812_sendByte(port, hi, lo, expand[(p >> 2) & 0x03]);  // B
  }
  sei();
  delayMicroseconds(80);
}

