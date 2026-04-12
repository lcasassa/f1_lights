// sim_bridge.cpp – thin C API around the Arduino sketch for Python ctypes.
//
// Build as a shared library (via Makefile):
//   make sim
//
#include "Arduino.h"

// ── The sketch provides these two symbols (in main.cpp) ─────────────────────
void setup();
void loop();

extern "C" {

// ── Sketch entry-points ─────────────────────────────────────────────────────
void sim_setup()              { setup(); }
void sim_loop()               { loop();  }

// ── Clock control ───────────────────────────────────────────────────────────
void          sim_set_millis(unsigned long ms) { sim_millis_value = ms; }
unsigned long sim_get_millis()                 { return sim_millis_value; }
void          sim_advance_millis(unsigned long delta) { sim_millis_value += delta; }

// ── Pin / LED inspection ────────────────────────────────────────────────────
//  pin_value == what was last written with digitalWrite  (HIGH/LOW)
//  pin_input == what digitalRead will return  (set from Python to simulate buttons)

uint8_t sim_get_pin_value(uint8_t pin) {
    return (pin < SIM_MAX_PINS) ? sim_pin_values[pin] : 0;
}

void sim_set_pin_input(uint8_t pin, uint8_t value) {
    if (pin < SIM_MAX_PINS) sim_pin_input[pin] = value;
}

uint8_t sim_get_pin_input(uint8_t pin) {
    return (pin < SIM_MAX_PINS) ? sim_pin_input[pin] : 0;
}

uint8_t sim_get_pin_mode(uint8_t pin) {
    return (pin < SIM_MAX_PINS) ? sim_pin_modes[pin] : 0;
}

// ── Bulk helpers (useful from Python) ───────────────────────────────────────
// Copies all 32 pin output values into the caller-supplied buffer.
void sim_get_all_pin_values(uint8_t* out, uint8_t count) {
    for (uint8_t i = 0; i < count && i < SIM_MAX_PINS; i++) {
        out[i] = sim_pin_values[i];
    }
}

// ── Tone / buzzer inspection ────────────────────────────────────────────────
// Auto-expire: if a duration was specified and the sim clock has passed it,
// the tone is silenced — matching real Arduino hardware behaviour.
unsigned int sim_get_tone_freq() {
    if (sim_tone_freq != 0 && sim_tone_end_ms != 0 && sim_millis_value >= sim_tone_end_ms) {
        sim_tone_freq   = 0;
        sim_tone_end_ms = 0;
    }
    return sim_tone_freq;
}
uint8_t      sim_get_tone_pin()  { return sim_tone_pin;  }

// ── Tone event log access ───────────────────────────────────────────────────
unsigned int  sim_get_tone_log_count()             { return sim_tone_log_count; }
unsigned long sim_get_tone_log_ms(unsigned int i)  { return (i < sim_tone_log_count) ? sim_tone_log[i].ms   : 0; }
unsigned int  sim_get_tone_log_freq(unsigned int i){ return (i < sim_tone_log_count) ? sim_tone_log[i].freq : 0; }
void          sim_tone_log_clear()                 { sim_tone_log_count = 0; }

// ── Full reset (re-initialise all simulated state) ──────────────────────────
void sim_reset() {
    for (uint8_t i = 0; i < SIM_MAX_PINS; i++) {
        sim_pin_values[i] = 0;
        sim_pin_modes[i]  = 0;
        sim_pin_input[i]  = 0;
    }
    sim_millis_value = 0;
    sim_tone_freq   = 0;
    sim_tone_pin    = 0;
    sim_tone_end_ms = 0;
    sim_tone_log_count = 0;
}

}  // extern "C"

