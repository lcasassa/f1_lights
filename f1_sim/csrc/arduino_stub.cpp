#include "Arduino.h"
#include "Wire.h"
#include <cstdlib>

// ── Global simulated state ──────────────────────────────────────────────────
uint8_t sim_pin_values[SIM_MAX_PINS] = {};  // last digitalWrite value
uint8_t sim_pin_modes[SIM_MAX_PINS]  = {};  // configured pin mode
uint8_t sim_pin_input[SIM_MAX_PINS]  = {};  // what digitalRead returns

// ── I2C / Wire stub globals ─────────────────────────────────────────────────
TwoWire Wire;
uint8_t sim_i2c_buf[SIM_I2C_BUF_SIZE] = {};
uint8_t sim_i2c_buf_len = 0;
uint8_t sim_i2c_display_buf[16] = {};

unsigned long sim_millis_value = 0;

unsigned int   sim_tone_freq   = 0;
uint8_t        sim_tone_pin    = 0;
unsigned long  sim_tone_end_ms = 0;

SimToneEvent  sim_tone_log[SIM_TONE_LOG_MAX] = {};
unsigned int  sim_tone_log_count = 0;

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
    // Advance clock by 1ms per read to prevent busy-wait loops from hanging
    // (on real hardware, millis() advances via hardware timer during busy-waits).
    sim_millis_value++;
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

static void sim_tone_log_push(unsigned int freq);

void delay(unsigned long ms) {
    unsigned long target = sim_millis_value + ms;
    // If a timed tone expires during this delay, log the silence at expiry time
    if (sim_tone_freq != 0 && sim_tone_end_ms != 0 && sim_tone_end_ms <= target && sim_tone_end_ms > sim_millis_value) {
        sim_millis_value = sim_tone_end_ms;
        sim_tone_freq = 0;
        sim_tone_end_ms = 0;
        sim_tone_log_push(0);
    }
    sim_millis_value = target;
}
void delayMicroseconds(unsigned int) { /* no-op */ }

static void sim_tone_log_push(unsigned int freq) {
    if (sim_tone_log_count < SIM_TONE_LOG_MAX) {
        sim_tone_log[sim_tone_log_count++] = { sim_millis_value, freq };
    }
}

void tone(uint8_t pin, unsigned int freq, unsigned long duration) {
    sim_tone_pin    = pin;
    sim_tone_freq   = freq;
    sim_tone_end_ms = (duration > 0) ? (sim_millis_value + duration) : 0;
    sim_tone_log_push(freq);
}
void noTone(uint8_t pin) {
    if (pin == sim_tone_pin) {
        sim_tone_freq   = 0;
        sim_tone_end_ms = 0;
        sim_tone_log_push(0);
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

