/**
 * f1sim.js — JS wrapper around the WASM simulation module.
 * Mirrors the Python F1Sim class API.
 */

// Pin mapping — must match ht16k33_display.h ledPins_[]
const A0 = 14
const A1 = 15
const PIN_MAP = [6, 8, A1, 11, 10, 4, 7, 9, A0, 12] // L1..L10
const BUTTON_LEFT_PIN = 2   // BTN_B in main.cpp
const BUTTON_RIGHT_PIN = 3  // BTN_A in main.cpp
const HIGH = 1
const LOW = 0

// ── HT16K33 segment map (from ht16k33_display.h) ───────────────────────────
// segMap[digit][segment] = [byteIdx, bitIdx]   (0xFF = unavailable)
const UNAVAIL = 0xFF
const SEG_MAP = [
  // Digit 1
  [[0,5],[1,1],[0,6],[0,1],[0,0],[1,0],[1,2],[UNAVAIL,0]],
  // Digit 2
  [[2,5],[3,1],[2,6],[2,1],[2,0],[3,0],[3,2],[UNAVAIL,0]],
  // Digit 3
  [[4,5],[5,1],[4,6],[4,1],[4,0],[5,0],[5,2],[UNAVAIL,0]],
  // Digit 4
  [[6,5],[7,1],[6,6],[6,1],[6,0],[7,0],[7,2],[UNAVAIL,0]],
  // Digit 5
  [[0,2],[0,3],[1,5],[0,4],[1,3],[1,6],[0,7],[1,4]],
  // Digit 6
  [[2,2],[2,3],[3,5],[2,4],[3,3],[3,6],[2,7],[3,4]],
  // Digit 7
  [[4,2],[4,3],[5,5],[4,4],[5,3],[5,6],[4,7],[5,4]],
  // Digit 8
  [[6,2],[6,3],[7,5],[6,4],[7,3],[7,6],[6,7],[7,4]],
]

/**
 * Decode 16-byte I2C display buffer into per-digit segment bitmasks.
 * Returns array of 8 numbers, each with bits: A=0, B=1, C=2, D=3, E=4, F=5, G=6, DP=7
 */
function decodeDisplayBuf(buf) {
  const digits = []
  for (let d = 0; d < 8; d++) {
    let segs = 0
    for (let s = 0; s < 8; s++) {
      const [byteIdx, bitIdx] = SEG_MAP[d][s]
      if (byteIdx === UNAVAIL) continue
      if (buf[byteIdx] & (1 << bitIdx)) {
        segs |= (1 << s)
      }
    }
    digits.push(segs)
  }
  return digits
}

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
  const simGetDisplayByte = mod.cwrap('sim_get_display_byte', 'number', ['number'])

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

    /**
     * Read the 16-byte HT16K33 display buffer and decode it into
     * an array of 8 segment bitmasks (one per digit).
     * Bits: A=0, B=1, C=2, D=3, E=4, F=5, G=6, DP=7
     */
    displayDigits() {
      const buf = new Uint8Array(16)
      for (let i = 0; i < 16; i++) buf[i] = simGetDisplayByte(i)
      return decodeDisplayBuf(buf)
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

