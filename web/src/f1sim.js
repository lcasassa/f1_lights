/**
 * f1sim.js — JS wrapper around the WASM simulation module.
 * Mirrors the Python F1Sim class API.
 */

// Pin mapping — must match main.cpp
const A0 = 14
const A1 = 15
const PIN_MAP = [4, 11, 12, A0, A1, 5, 10, 9, 8, 7] // POS-1..POS-10
const BUTTON_LEFT_PIN = 2
const BUTTON_RIGHT_PIN = 3
const HIGH = 1
const LOW = 0

export function createSim() {
  const mod = window.__f1sim
  if (!mod) throw new Error('WASM module not loaded')

  const simSetup = mod.cwrap('sim_setup', null, [])
  const simLoop = mod.cwrap('sim_loop', null, [])
  const simSetMillis = mod.cwrap('sim_set_millis', null, ['number'])
  const simGetMillis = mod.cwrap('sim_get_millis', 'number', [])
  const simAdvanceMillis = mod.cwrap('sim_advance_millis', null, ['number'])
  const simGetPinValue = mod.cwrap('sim_get_pin_value', 'number', ['number'])
  const simSetPinInput = mod.cwrap('sim_set_pin_input', null, ['number', 'number'])
  const simReset = mod.cwrap('sim_reset', null, [])
  const simGetToneFreq = mod.cwrap('sim_get_tone_freq', 'number', [])
  const simGetTonePin = mod.cwrap('sim_get_tone_pin', 'number', [])
  const simGetToneLogCount = mod.cwrap('sim_get_tone_log_count', 'number', [])
  const simGetToneLogMs = mod.cwrap('sim_get_tone_log_ms', 'number', ['number'])
  const simGetToneLogFreq = mod.cwrap('sim_get_tone_log_freq', 'number', ['number'])
  const simToneLogClear = mod.cwrap('sim_tone_log_clear', null, [])

  return {
    setup: () => simSetup(),
    loop: () => simLoop(),
    setMillis: (ms) => simSetMillis(ms),
    getMillis: () => simGetMillis(),
    advanceMillis: (delta) => simAdvanceMillis(delta),
    reset: () => simReset(),

    // Tone / buzzer helpers
    toneFreq: () => simGetToneFreq(),
    tonePin: () => simGetTonePin(),

    /** Return the full tone event log as an array of {ms, freq} objects. */
    toneLog() {
      const n = simGetToneLogCount()
      const log = []
      for (let i = 0; i < n; i++) {
        log.push({ ms: simGetToneLogMs(i), freq: simGetToneLogFreq(i) })
      }
      return log
    },
    toneLogClear: () => simToneLogClear(),

    // LED helpers
    ledState(position) {
      if (position < 1 || position > 10) throw new Error('position must be 1-10')
      const pin = PIN_MAP[position - 1]
      return simGetPinValue(pin) === HIGH
    },

    ledStates() {
      const states = {}
      for (let i = 1; i <= 10; i++) states[i] = this.ledState(i)
      return states
    },

    topRow() {
      return [1, 2, 3, 4, 5].map((p) => this.ledState(p))
    },

    bottomRow() {
      return [6, 7, 8, 9, 10].map((p) => this.ledState(p))
    },

    // Button helpers
    pressLeft: () => simSetPinInput(BUTTON_LEFT_PIN, LOW),
    releaseLeft: () => simSetPinInput(BUTTON_LEFT_PIN, HIGH),
    pressRight: () => simSetPinInput(BUTTON_RIGHT_PIN, LOW),
    releaseRight: () => simSetPinInput(BUTTON_RIGHT_PIN, HIGH),

    pressBoth() {
      this.pressLeft()
      this.pressRight()
    },

    releaseBoth() {
      this.releaseLeft()
      this.releaseRight()
    },
  }
}

