#pragma once
// Arduino API stub for desktop simulation
// Provides just enough of the Arduino API to compile main.cpp on a host computer.

#include <cstdint>
#include <cstddef>

// ── Pin mode / digital constants ────────────────────────────────────────────
constexpr uint8_t LOW  = 0;
constexpr uint8_t HIGH = 1;
constexpr uint8_t OUTPUT       = 0x01;
constexpr uint8_t INPUT        = 0x00;
constexpr uint8_t INPUT_PULLUP = 0x02;

// ── Analog pin aliases (match AVR Arduino core) ─────────────────────────────
constexpr uint8_t A0 = 14;
constexpr uint8_t A1 = 15;
constexpr uint8_t A2 = 16;
constexpr uint8_t A3 = 17;
constexpr uint8_t A4 = 18;
constexpr uint8_t A5 = 19;

// ── Maximum number of simulated pins ────────────────────────────────────────
constexpr uint8_t SIM_MAX_PINS = 32;

// ── Pin state accessible from the outside ───────────────────────────────────
// pin_values[pin] holds the last written digital value (HIGH/LOW).
// pin_modes[pin]  holds the configured mode (INPUT / OUTPUT / INPUT_PULLUP).
// pin_input[pin]  holds the value that digitalRead will return (set externally).
extern uint8_t sim_pin_values[SIM_MAX_PINS];
extern uint8_t sim_pin_modes[SIM_MAX_PINS];
extern uint8_t sim_pin_input[SIM_MAX_PINS];

// ── Simulated clock ─────────────────────────────────────────────────────────
extern unsigned long sim_millis_value;

// ── Arduino digital / analog API ────────────────────────────────────────────
void     pinMode(uint8_t pin, uint8_t mode);
void     digitalWrite(uint8_t pin, uint8_t val);
int      digitalRead(uint8_t pin);
int      analogRead(uint8_t pin);

// ── Time API ────────────────────────────────────────────────────────────────
unsigned long millis();
void          delay(unsigned long ms);          // no-op in sim
void          delayMicroseconds(unsigned int us); // no-op

// ── Tone API (passive buzzer) ────────────────────────────────────────────────
void tone(uint8_t pin, unsigned int frequency, unsigned long duration = 0);
void noTone(uint8_t pin);

// ── Random API ──────────────────────────────────────────────────────────────
void  randomSeed(unsigned long seed);
long  random(long max);
long  random(long min, long max);

// ── Serial stub ─────────────────────────────────────────────────────────────
#include <cstdio>
#include <cstring>

class HardwareSerial {
public:
    void begin(unsigned long) {}
    void end() {}

    // print overloads
    void print(const char* s)        { /* silent in sim */ }
    void print(char c)               { /* silent */ }
    void print(int n)                { /* silent */ }
    void print(unsigned int n)       { /* silent */ }
    void print(long n)               { /* silent */ }
    void print(unsigned long n)      { /* silent */ }
    void print(double n, int = 2)    { /* silent */ }

    // println overloads
    void println()                   { /* silent */ }
    void println(const char* s)      { /* silent */ }
    void println(char c)             { /* silent */ }
    void println(int n)              { /* silent */ }
    void println(unsigned int n)     { /* silent */ }
    void println(long n)             { /* silent */ }
    void println(unsigned long n)    { /* silent */ }
    void println(double n, int = 2)  { /* silent */ }
};

extern HardwareSerial Serial;

