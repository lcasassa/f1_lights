#include "Arduino.h"
#include <cstdlib>

// ── Global simulated state ──────────────────────────────────────────────────
uint8_t sim_pin_values[SIM_MAX_PINS] = {};  // last digitalWrite value
uint8_t sim_pin_modes[SIM_MAX_PINS]  = {};  // configured pin mode
uint8_t sim_pin_input[SIM_MAX_PINS]  = {};  // what digitalRead returns

unsigned long sim_millis_value = 0;

unsigned int   sim_tone_freq   = 0;
uint8_t        sim_tone_pin    = 0;
unsigned long  sim_tone_end_ms = 0;

HardwareSerial Serial;

// ── Arduino API implementations ─────────────────────────────────────────────

void pinMode(uint8_t pin, uint8_t mode) {
    if (pin < SIM_MAX_PINS) {
        sim_pin_modes[pin] = mode;
        // INPUT_PULLUP: default read value is HIGH
        if (mode == INPUT_PULLUP) {
            sim_pin_input[pin] = HIGH;
        }
    }
}

void digitalWrite(uint8_t pin, uint8_t val) {
    if (pin < SIM_MAX_PINS) {
        sim_pin_values[pin] = val;
    }
}

int digitalRead(uint8_t pin) {
    if (pin < SIM_MAX_PINS) {
        return sim_pin_input[pin];
    }
    return HIGH;
}

int analogRead(uint8_t pin) {
    // Return a pseudo-random value (used only for randomSeed)
    return rand() % 1024;
}

unsigned long millis() {
    return sim_millis_value;
}

void delay(unsigned long) { /* no-op */ }
void delayMicroseconds(unsigned int) { /* no-op */ }

void tone(uint8_t pin, unsigned int freq, unsigned long duration) {
    sim_tone_pin    = pin;
    sim_tone_freq   = freq;
    sim_tone_end_ms = (duration > 0) ? (sim_millis_value + duration) : 0;
}
void noTone(uint8_t pin) {
    if (pin == sim_tone_pin) {
        sim_tone_freq   = 0;
        sim_tone_end_ms = 0;
    }
}

void randomSeed(unsigned long seed) {
    srand(static_cast<unsigned int>(seed));
}

long random(long max) {
    if (max <= 0) return 0;
    return rand() % max;
}

long random(long min, long max) {
    if (max <= min) return min;
    return min + (rand() % (max - min));
}

