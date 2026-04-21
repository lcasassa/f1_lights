<template>
  <div class="app">
    <h1 class="title">F1 START LIGHTS</h1>
    <div class="player-row">
      <div
        class="key"
        :class="{ active: leftPressed }"
        @mousedown.prevent="onBtnDown('left')"
        @mouseup.prevent="onBtnUp('left')"
        @mouseleave="onBtnUp('left')"
        @touchstart.prevent="onBtnDown('left')"
        @touchend.prevent="onBtnUp('left')"
        @touchcancel="onBtnUp('left')"
      >[1] Player B</div>
      <SevenSegDisplay :digits="segDigitsB" />
    </div>
    <F1Board :leds="leds" />
    <div class="player-row">
      <SevenSegDisplay :digits="segDigitsA" />
      <div
        class="key"
        :class="{ active: rightPressed }"
        @mousedown.prevent="onBtnDown('right')"
        @mouseup.prevent="onBtnUp('right')"
        @mouseleave="onBtnUp('right')"
        @touchstart.prevent="onBtnDown('right')"
        @touchend.prevent="onBtnUp('right')"
        @touchcancel="onBtnUp('right')"
      >[2] Player A</div>
    </div>
    <p class="hint">Hold <kbd>1</kbd> / <kbd>2</kbd> keys or tap &amp; hold the buttons</p>
    <footer class="footer">
      <a href="https://github.com/lcasassa/f1_lights" target="_blank" rel="noopener">GitHub</a>
      <span>·</span>
      <a href="https://github.com/lcasassa/f1_lights/blob/main/LICENSE" target="_blank" rel="noopener">MIT License</a>
      <span>·</span>
      <span>© 2026 Linus Casassa</span>
    </footer>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'
import { createSim } from './f1sim.js'
import F1Board from './components/F1Board.vue'
import SevenSegDisplay from './components/SevenSegDisplay.vue'

const LOOP_INTERVAL_MS = 20

const leds = ref(Array(10).fill(false))
const segDigitsA = ref(Array(4).fill(0))
const segDigitsB = ref(Array(4).fill(0))
const leftPressed = ref(false)
const rightPressed = ref(false)

let sim = null
let startRealMs = 0
let simTimeMs = 0
let rafId = null
let startupDone = false
let freezeUntilRealMs = 0        // pause game loop while a blocking sound plays
let frozenSimTimeMs = 0          // sim time when the freeze started

// ── Web Audio for buzzer tones ──────────────────────────────────────────────
let audioCtx = null
let oscillator = null
let gainNode = null
let currentFreq = 0

function ensureAudioCtx() {
  if (audioCtx) return
  audioCtx = new (window.AudioContext || window.webkitAudioContext)()
  gainNode = audioCtx.createGain()
  gainNode.gain.value = 0.12 // keep volume moderate
  gainNode.connect(audioCtx.destination)
}

function setTone(freq) {
  if (freq === currentFreq) return
  currentFreq = freq

  if (freq === 0) {
    // Stop
    if (oscillator) {
      oscillator.stop()
      oscillator.disconnect()
      oscillator = null
    }
    return
  }

  ensureAudioCtx()

  if (oscillator) {
    // Just update frequency on the existing oscillator
    oscillator.frequency.setValueAtTime(freq, audioCtx.currentTime)
  } else {
    oscillator = audioCtx.createOscillator()
    oscillator.type = 'square'
    oscillator.frequency.setValueAtTime(freq, audioCtx.currentTime)
    oscillator.connect(gainNode)
    oscillator.start()
  }
}

/**
 * Schedule a pre-recorded tone log with Web Audio.
 * Each entry is {ms, freq}. Uses a single oscillator with frequency
 * automation and a dedicated gain node for mute/unmute, so there are
 * no click artefacts between notes.
 * Returns the total duration in seconds.
 */
function scheduleToneLog(log) {
  if (!log.length) return 0
  ensureAudioCtx()

  const baseTime = audioCtx.currentTime
  const originMs = log[0].ms
  const lastMs = log[log.length - 1].ms
  const totalSec = (lastMs - originMs) / 1000

  // Dedicated gain node for this scheduled sequence (mute/unmute)
  const seqGain = audioCtx.createGain()
  seqGain.gain.setValueAtTime(0, baseTime) // start silent
  seqGain.connect(gainNode)

  // Single oscillator for the whole sequence
  const osc = audioCtx.createOscillator()
  osc.type = 'square'
  osc.frequency.setValueAtTime(200, baseTime) // arbitrary initial freq
  osc.connect(seqGain)
  osc.start(baseTime)
  osc.stop(baseTime + totalSec + 0.05) // small tail to avoid cutting last note

  for (let i = 0; i < log.length; i++) {
    const { ms, freq } = log[i]
    const timeSec = baseTime + (ms - originMs) / 1000

    if (freq === 0) {
      // Silence — mute the gain
      seqGain.gain.setValueAtTime(0, timeSec)
    } else {
      // Note — set frequency and unmute
      osc.frequency.setValueAtTime(freq, timeSec)
      seqGain.gain.setValueAtTime(1, timeSec)
    }
  }

  return totalSec
}
// ─────────────────────────────────────────────────────────────────────────────

const keysDown = new Set()

function onKeyDown(e) {
  if (e.repeat) return
  keysDown.add(e.key)
  updateButtons()
  // Resume AudioContext on user gesture (browser policy)
  if (audioCtx && audioCtx.state === 'suspended') audioCtx.resume()
  ensureAudioCtx()
}

function onKeyUp(e) {
  keysDown.delete(e.key)
  updateButtons()
}

// ── Touch / click button handling (mobile) ──────────────────────────────────
const touchDown = { left: false, right: false }

function onBtnDown(side) {
  touchDown[side] = true
  updateButtons()
  // Resume AudioContext on user gesture (browser policy)
  if (audioCtx && audioCtx.state === 'suspended') audioCtx.resume()
  ensureAudioCtx()
}

function onBtnUp(side) {
  touchDown[side] = false
  updateButtons()
}
// ─────────────────────────────────────────────────────────────────────────────

function updateButtons() {
  const left = keysDown.has('1') || touchDown.left
  const right = keysDown.has('2') || touchDown.right

  leftPressed.value = left
  rightPressed.value = right

  if (!sim || !startupDone) return
  if (left) sim.pressLeft()
  else sim.releaseLeft()
  if (right) sim.pressRight()
  else sim.releaseRight()
}

function readSimState() {
  if (!sim) return
  const newLeds = []
  for (let i = 1; i <= 10; i++) {
    newLeds.push(sim.ledState(i))
  }
  leds.value = newLeds
  try {
    const all = sim.displayDigits()
    segDigitsA.value = all.slice(0, 4)
    segDigitsB.value = all.slice(4, 8)
  } catch (e) {
    // displayDigits may not be available if WASM was built without the export
  }
}

function tick() {
  rafId = requestAnimationFrame(tick)

  if (!sim) return

  const nowReal = performance.now()

  // If we're frozen (blocking sound playing), wait it out then resync
  if (freezeUntilRealMs > 0) {
    if (nowReal < freezeUntilRealMs) {
      // Still frozen — update display state but don't advance sim
      readSimState()
      return
    }
    // Freeze ended — resync real-time base so that the game continues
    // from the post-sound sim time without any catchup burst.
    simTimeMs = frozenSimTimeMs
    startRealMs = nowReal - simTimeMs
    freezeUntilRealMs = 0
    frozenSimTimeMs = 0
  }

  const realElapsedMs = nowReal - startRealMs
  while (simTimeMs + LOOP_INTERVAL_MS <= realElapsedMs) {
    simTimeMs += LOOP_INTERVAL_MS
    sim.setMillis(Math.floor(simTimeMs))
      sim.toneLogClear()
    sim.loop()

    // If loop() contained blocking tone sequences (e.g. buzzerWinnerChirp),
    // the sim clock jumped ahead inside delay(). Drain the tone log,
    // schedule the sound via Web Audio, and freeze the game loop until
    // the sound finishes playing in real time.
    const actualSimMs = sim.getMillis()
    if (actualSimMs > simTimeMs + LOOP_INTERVAL_MS) {
      const log = sim.toneLog()
      let durationSec = 0
      if (log.length) {
        durationSec = scheduleToneLog(log)
        sim.toneLogClear()
      }
      // Stop the persistent oscillator so it doesn't drone over the scheduled sound
      setTone(0)
      // Freeze the game loop for the sound duration
      frozenSimTimeMs = actualSimMs
      freezeUntilRealMs = performance.now() + durationSec * 1000
      break
    }
  }

  // Read LED states
  readSimState()

  // Sync buzzer tone (for non-blocking tones like beeps)
  if (freezeUntilRealMs === 0) {
    setTone(sim.toneFreq())
  }
}

onMounted(() => {
  sim = createSim()
  sim.reset()
  sim.setMillis(0)
  sim.setup()  // runs the blocking startup sound — advances sim clock & fills tone log

  // Read the startup tone log produced during setup() and schedule it with Web Audio
  ensureAudioCtx()
  const startupLog = sim.toneLog()
  const startupDurationSec = scheduleToneLog(startupLog)
  sim.toneLogClear()

  // The sim clock is now at the end of setup(). Sync simTimeMs to it and
  // start the real-time loop after the startup sound finishes playing.
  const postSetupMs = sim.getMillis()

  setTimeout(() => {
    startupDone = true
    simTimeMs = postSetupMs
    startRealMs = performance.now()
    rafId = requestAnimationFrame(tick)
    // Read initial display state so the READY animation is visible immediately
    readSimState()
  }, startupDurationSec * 1000)

  window.addEventListener('keydown', onKeyDown)
  window.addEventListener('keyup', onKeyUp)
})

onUnmounted(() => {
  if (rafId) cancelAnimationFrame(rafId)
  window.removeEventListener('keydown', onKeyDown)
  window.removeEventListener('keyup', onKeyUp)
  setTone(0)
  if (audioCtx) audioCtx.close()
})
</script>

<style>
.app {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 24px;
  user-select: none;
}

.title {
  color: #ccc;
  font-size: 22px;
  font-weight: bold;
  letter-spacing: 2px;
  margin: 0;
}

.player-row {
  display: flex;
  align-items: center;
  gap: 16px;
}


.key {
  padding: 14px 28px;
  border-radius: 6px;
  background: #222;
  color: #555;
  font-size: 18px;
  font-family: 'Courier New', monospace;
  border: 1px solid #333;
  transition: all 0.1s;
  cursor: pointer;
  -webkit-tap-highlight-color: transparent;
  touch-action: manipulation;
}

.key.active {
  background: #444;
  color: #dcdc3c;
  border-color: #dcdc3c;
}

.hint {
  color: #555;
  font-size: 13px;
  margin: 0;
}

kbd {
  background: #333;
  color: #aaa;
  padding: 1px 6px;
  border-radius: 3px;
  border: 1px solid #555;
  font-family: inherit;
}

.footer {
  display: flex;
  gap: 8px;
  align-items: center;
  margin-top: 12px;
  font-size: 12px;
  color: #444;
}

.footer a {
  color: #666;
  text-decoration: none;
}

.footer a:hover {
  color: #999;
  text-decoration: underline;
}
</style>

