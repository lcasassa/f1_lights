<template>
  <div class="app">
    <h1 class="title">F1 START LIGHTS</h1>
    <F1Board :leds="leds" />
    <div class="controls">
      <div class="key" :class="{ active: leftPressed }">[1] Player 1</div>
      <div class="key" :class="{ active: rightPressed }">[2] Player 2</div>
    </div>
    <p class="hint">Hold <kbd>1</kbd> / <kbd>2</kbd> to press buttons</p>
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

const keysDown = new Set()

function onKeyDown(e) {
  if (e.repeat) return
  keysDown.add(e.key)
  updateButtons()
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

  if (!sim) return
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
}

onMounted(() => {
  sim = createSim()
  sim.reset()
  sim.setMillis(0)
  sim.setup()

  startRealMs = performance.now()
  simTimeMs = 0

  window.addEventListener('keydown', onKeyDown)
  window.addEventListener('keyup', onKeyUp)

  rafId = requestAnimationFrame(tick)
})

onUnmounted(() => {
  if (rafId) cancelAnimationFrame(rafId)
  window.removeEventListener('keydown', onKeyDown)
  window.removeEventListener('keyup', onKeyUp)
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
</style>

