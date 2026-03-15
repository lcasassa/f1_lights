// sim_bridge.cpp – thin C API around the Arduino sketch for Python ctypes.
//
// Build as a shared library:
//   c++ -std=c++17 -shared -fPIC -I sim sim/arduino_stub.cpp sim/sim_bridge.cpp src/main.cpp -o libf1sim.dylib
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

// ── Full reset (re-initialise all simulated state) ──────────────────────────
void sim_reset() {
    for (uint8_t i = 0; i < SIM_MAX_PINS; i++) {
        sim_pin_values[i] = 0;
        sim_pin_modes[i]  = 0;
        sim_pin_input[i]  = 0;
    }
    sim_millis_value = 0;
}

}  // extern "C"

