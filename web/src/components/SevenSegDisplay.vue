<template>
  <canvas
    ref="canvas"
    :width="canvasWidth"
    :height="canvasHeight"
    class="seg-display"
  />
</template>

<script setup>
import { ref, watch, onMounted } from 'vue'

const props = defineProps({
  /** Array of 4 segment bitmasks. Bits: A=0 B=1 C=2 D=3 E=4 F=5 G=6 DP=7 */
  digits: { type: Array, required: true },
})

// ── Layout constants ────────────────────────────────────────────────────────
const DIGIT_W = 36
const DIGIT_H = 60
const DIGIT_GAP = 8
const SEG_THICK = 5
const PAD_X = 16
const PAD_Y = 14
const DP_RADIUS = 3

const groupWidth = 4 * DIGIT_W + 3 * DIGIT_GAP
const canvasWidth = PAD_X * 2 + groupWidth
const canvasHeight = PAD_Y * 2 + DIGIT_H

const canvas = ref(null)

const SEG_ON  = '#ff3333'
const SEG_OFF = '#1a0808'
const BG      = '#0a0a0a'

function drawSegH(ctx, x, y, w) {
  const h = SEG_THICK
  const t = h / 2
  ctx.beginPath()
  ctx.moveTo(x + t, y)
  ctx.lineTo(x + w - t, y)
  ctx.lineTo(x + w, y + t)
  ctx.lineTo(x + w - t, y + h)
  ctx.lineTo(x + t, y + h)
  ctx.lineTo(x, y + t)
  ctx.closePath()
  ctx.fill()
}

function drawSegV(ctx, x, y, h) {
  const w = SEG_THICK
  const t = w / 2
  ctx.beginPath()
  ctx.moveTo(x + t, y)
  ctx.lineTo(x + w, y + t)
  ctx.lineTo(x + w, y + h - t)
  ctx.lineTo(x + t, y + h)
  ctx.lineTo(x, y + h - t)
  ctx.lineTo(x, y + t)
  ctx.closePath()
  ctx.fill()
}

function drawDigit(ctx, ox, oy, segs) {
  const w = DIGIT_W
  const halfH = (DIGIT_H - SEG_THICK) / 2
  const margin = 2

  ctx.fillStyle = (segs & 0x01) ? SEG_ON : SEG_OFF
  drawSegH(ctx, ox + margin, oy, w - margin * 2)

  ctx.fillStyle = (segs & 0x02) ? SEG_ON : SEG_OFF
  drawSegV(ctx, ox + w - SEG_THICK, oy + margin, halfH - margin)

  ctx.fillStyle = (segs & 0x04) ? SEG_ON : SEG_OFF
  drawSegV(ctx, ox + w - SEG_THICK, oy + halfH + SEG_THICK + margin, halfH - margin)

  ctx.fillStyle = (segs & 0x08) ? SEG_ON : SEG_OFF
  drawSegH(ctx, ox + margin, oy + DIGIT_H - SEG_THICK, w - margin * 2)

  ctx.fillStyle = (segs & 0x10) ? SEG_ON : SEG_OFF
  drawSegV(ctx, ox, oy + halfH + SEG_THICK + margin, halfH - margin)

  ctx.fillStyle = (segs & 0x20) ? SEG_ON : SEG_OFF
  drawSegV(ctx, ox, oy + margin, halfH - margin)

  ctx.fillStyle = (segs & 0x40) ? SEG_ON : SEG_OFF
  drawSegH(ctx, ox + margin, oy + halfH, w - margin * 2)

  ctx.fillStyle = (segs & 0x80) ? SEG_ON : SEG_OFF
  ctx.beginPath()
  ctx.arc(ox + w + DP_RADIUS + 2, oy + DIGIT_H - DP_RADIUS - 1, DP_RADIUS, 0, Math.PI * 2)
  ctx.fill()
}

function draw() {
  const el = canvas.value
  if (!el) return
  const ctx = el.getContext('2d')

  ctx.fillStyle = BG
  ctx.fillRect(0, 0, canvasWidth, canvasHeight)

  // Rounded bezel
  const bezelPad = 6
  ctx.fillStyle = '#111'
  const gx = PAD_X - bezelPad
  const gy = PAD_Y - bezelPad
  const bw = groupWidth + bezelPad * 2
  const bh = DIGIT_H + bezelPad * 2
  const r = 6
  ctx.beginPath()
  ctx.moveTo(gx + r, gy)
  ctx.lineTo(gx + bw - r, gy)
  ctx.quadraticCurveTo(gx + bw, gy, gx + bw, gy + r)
  ctx.lineTo(gx + bw, gy + bh - r)
  ctx.quadraticCurveTo(gx + bw, gy + bh, gx + bw - r, gy + bh)
  ctx.lineTo(gx + r, gy + bh)
  ctx.quadraticCurveTo(gx, gy + bh, gx, gy + bh - r)
  ctx.lineTo(gx, gy + r)
  ctx.quadraticCurveTo(gx, gy, gx + r, gy)
  ctx.closePath()
  ctx.fill()

  // Draw 4 digits
  const digits = props.digits
  for (let i = 0; i < 4; i++) {
    const ox = PAD_X + i * (DIGIT_W + DIGIT_GAP)
    drawDigit(ctx, ox, PAD_Y, digits[i] ?? 0)
  }
}

onMounted(() => draw())
watch(() => props.digits, () => draw(), { deep: true })
</script>

<style scoped>
.seg-display {
  border-radius: 8px;
  border: 1px solid #222;
}
</style>
