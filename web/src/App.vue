<template>
  <div class="app">
    <h1 class="title">F1 START LIGHTS</h1>
    <F1Board :leds="leds" />
    <div class="controls">
      <div class="key" :class="{ active: leftPressed }">[1] Player 1</div>
      <div class="key" :class="{ active: rightPressed }">[2] Player 2</div>
    </div>
    <p class="hint">Hold <kbd>1</kbd> / <kbd>2</kbd> to press buttons</p>
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

const LOOP_INTERVAL_MS = 20

const leds = ref(Array(10).fill(false))
const leftPressed = ref(false)
const rightPressed = ref(false)

let sim = null
let startRealMs = 0
let simTimeMs = 0
let rafId = null
let startupDone = false

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
 * Each entry is {ms, freq}. We create oscillator segments so the startup
 * sound plays back in real-time exactly as it would on the hardware.
 * Returns the total duration in seconds.
 */
function scheduleToneLog(log) {
  if (!log.length) return 0
  ensureAudioCtx()

  const baseTime = audioCtx.currentTime
  const originMs = log[0].ms

  for (let i = 0; i < log.length; i++) {
    const { ms, freq } = log[i]
    // Determine when the next event starts (or end of this one)
    const nextMs = (i + 1 < log.length) ? log[i + 1].ms : ms
    const startSec = (ms - originMs) / 1000
    const endSec = (nextMs - originMs) / 1000

    if (freq === 0 || endSec <= startSec) continue

    const osc = audioCtx.createOscillator()
    osc.type = 'square'
    osc.frequency.value = freq
    osc.connect(gainNode)
    osc.start(baseTime + startSec)
    osc.stop(baseTime + endSec)
  }

  const lastMs = log[log.length - 1].ms
  return (lastMs - originMs) / 1000
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

function updateButtons() {
  const left = keysDown.has('1')
  const right = keysDown.has('2')

  leftPressed.value = left
  rightPressed.value = right

  if (!sim || !startupDone) return
  if (left) sim.pressLeft()
  else sim.releaseLeft()
  if (right) sim.pressRight()
  else sim.releaseRight()
}

function tick() {
  rafId = requestAnimationFrame(tick)

  if (!sim) return

  const realElapsedMs = performance.now() - startRealMs
  while (simTimeMs + LOOP_INTERVAL_MS <= realElapsedMs) {
    simTimeMs += LOOP_INTERVAL_MS
    sim.setMillis(Math.floor(simTimeMs))
    sim.loop()
  }

  // Read LED states
  const newLeds = []
  for (let i = 1; i <= 10; i++) {
    newLeds.push(sim.ledState(i))
  }
  leds.value = newLeds

  // Sync buzzer tone
  setTone(sim.toneFreq())
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

.controls {
  display: flex;
  gap: 32px;
}

.key {
  padding: 8px 20px;
  border-radius: 6px;
  background: #222;
  color: #555;
  font-size: 16px;
  font-family: 'Courier New', monospace;
  border: 1px solid #333;
  transition: all 0.1s;
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

