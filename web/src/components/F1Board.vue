<template>
  <canvas
    ref="canvas"
    :width="canvasWidth"
    :height="canvasHeight"
    class="board"
  />
</template>

<script setup>
import { ref, watch, onMounted } from 'vue'

const props = defineProps({
  leds: { type: Array, required: true },
})

const LEDS_PER_ROW = 5
const ROWS = 2
const LED_RADIUS = 36
const LED_SPACING = 100
const PADDING = 60
const ROW_GAP = 110

const canvasWidth = PADDING * 2 + (LEDS_PER_ROW - 1) * LED_SPACING + LED_RADIUS * 2
const canvasHeight = PADDING * 2 + (ROWS - 1) * ROW_GAP + LED_RADIUS * 2

const canvas = ref(null)

function ledCentre(row, col) {
  const x = PADDING + LED_RADIUS + col * LED_SPACING
  const y = PADDING + LED_RADIUS + row * ROW_GAP
  return { x, y }
}

function draw() {
  const el = canvas.value
  if (!el) return
  const ctx = el.getContext('2d')

  // Background
  ctx.fillStyle = '#0f0f0f'
  ctx.fillRect(0, 0, canvasWidth, canvasHeight)

  for (let row = 0; row < ROWS; row++) {
    for (let col = 0; col < LEDS_PER_ROW; col++) {
      const pos = row * LEDS_PER_ROW + col // 0-indexed into props.leds
      const on = props.leds[pos] ?? false
      const { x, y } = ledCentre(row, col)

      // Glow
      if (on) {
        const glowR = LED_RADIUS + 8
        const grad = ctx.createRadialGradient(x, y, LED_RADIUS * 0.5, x, y, glowR)
        grad.addColorStop(0, 'rgba(255, 120, 60, 0.35)')
        grad.addColorStop(1, 'rgba(255, 120, 60, 0)')
        ctx.beginPath()
        ctx.arc(x, y, glowR, 0, Math.PI * 2)
        ctx.fillStyle = grad
        ctx.fill()
      }

      // LED body
      ctx.beginPath()
      ctx.arc(x, y, LED_RADIUS, 0, Math.PI * 2)
      ctx.fillStyle = on ? '#ff1e1e' : '#3c0a0a'
      ctx.fill()

      // Ring
      ctx.beginPath()
      ctx.arc(x, y, LED_RADIUS, 0, Math.PI * 2)
      ctx.strokeStyle = on ? '#ffffff' : '#5a1e1e'
      ctx.lineWidth = 2
      ctx.stroke()

      // Position label
      ctx.fillStyle = '#666'
      ctx.font = '13px Courier New'
      ctx.textAlign = 'center'
      ctx.fillText(String(pos + 1), x, y + LED_RADIUS + 16)
    }
  }
}

onMounted(() => draw())
watch(() => props.leds, () => draw(), { deep: true })
</script>

<style scoped>
.board {
  border-radius: 12px;
  border: 1px solid #222;
}
</style>

