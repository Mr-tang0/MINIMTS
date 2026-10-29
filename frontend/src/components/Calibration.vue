<template>
  <div class="calibration-page">
    <header class="toolbar">
      <div>
        <h1>{{ title }}</h1>
        <p>{{ hint }}</p>
      </div>
      <span class="zoom">{{ Math.round(scale * 100) }}%</span>
    </header>

    <main ref="stage" class="stage" @wheel.prevent="zoomImage">
      <canvas
        ref="canvas"
        @mousedown="mouseDown"
        @mousemove="mouseMove"
        @mouseup="mouseUp"
        @mouseleave="mouseUp"
      />
      <div v-if="loading" class="message">正在加载最新相机图像...</div>
      <div v-else-if="!imageReady" class="message error">未获取到相机图像</div>
    </main>

    <footer class="footer">
      <div class="status">
        <span>{{ statusLabel }}</span>
        <strong v-if="isCorners && corners.length">已检测 {{ corners.length }} 个角点</strong>
        <strong v-else-if="line">长度 {{ lineLength.toFixed(1) }} 像素</strong>
        <strong v-else-if="roi">X {{ Math.round(roi.x) }} / Y {{ Math.round(roi.y) }} / W {{ Math.round(roi.width) }} / H {{ Math.round(roi.height) }}</strong>
        <strong v-else>{{ emptyLabel }}</strong>
      </div>
      <label v-if="mode === 'calibration'" class="real-length">
        <span>实际长度</span>
        <input v-model.number="realLength" type="number" min="0.001" step="0.001">
      </label>
      <div class="actions">
        <button class="secondary" type="button" @click="close">取消</button>
        <button class="primary" type="button" :disabled="!canConfirm" @click="confirm">确认</button>
      </div>
    </footer>
  </div>
</template>

<script setup>
import { computed, nextTick, onMounted, onUnmounted, ref } from 'vue'
import { useRoute } from 'vue-router'
import { Events, Window } from '@wailsio/runtime'
import * as HIKCameraService from '../../bindings/MINIMTS/services/devices/hikcameraservice.js'
import * as HIKCalibrationService from '../../bindings/MINIMTS/services/devices/hikcalibrationservice.js'
import * as HIKExtensometerService from '../../bindings/MINIMTS/services/devices/hikextensometerservice.js'

const route = useRoute()
const mode = computed(() => route.query.mode || 'roi')
const label = computed(() => route.query.label === 'B' ? 'B' : 'A')
const isCorners = computed(() => mode.value === 'calibration_corners' || mode.value === 'pose_calibration')
const isLine = computed(() => mode.value === 'line' || mode.value === 'calibration')
const rows = computed(() => Number.parseInt(route.query.rows) || 7)
const cols = computed(() => Number.parseInt(route.query.cols) || 5)
const squareSize = computed(() => Number.parseFloat(route.query.squareSize) || 25)

const title = computed(() => ({
  roi: `选取标记 ${label.value}`,
  line: '绘制拉伸方向',
  calibration: '绘制比例标定线',
  calibration_corners: '相机标定：调整角点',
  pose_calibration: '位姿标定：调整角点'
}[mode.value] || '标定'))
const hint = computed(() => isCorners.value ? '拖动红色角点调整位置，滚轮缩放图片' : isLine.value ? '左键拖拽绘制线段，滚轮缩放图片' : '左键拖拽绘制 ROI，滚轮缩放图片')
const statusLabel = computed(() => isCorners.value ? '棋盘格角点' : isLine.value ? '标定线段' : 'ROI')
const emptyLabel = computed(() => isCorners.value ? '未检测到角点' : isLine.value ? '请绘制线段' : '请绘制矩形')
const canConfirm = computed(() => {
  if (isCorners.value) return corners.value.length > 0
  if (mode.value === 'calibration') return !!line.value && realLength.value > 0
  if (isLine.value) return !!line.value
  return !!roi.value
})
const lineLength = computed(() => line.value ? Math.hypot(line.value.x2 - line.value.x1, line.value.y2 - line.value.y1) : 0)

const stage = ref(null)
const canvas = ref(null)
const loading = ref(true)
const imageReady = ref(false)
const roi = ref(null)
const line = ref(null)
const corners = ref([])
const scale = ref(1)
const realLength = ref(0)
let image = new Image()
let imageWidth = 0
let imageHeight = 0
let offsetX = 0
let offsetY = 0
let drawing = false
let start = null
let current = null
let draggingCorner = -1

const imagePoint = (event) => {
  const rect = canvas.value.getBoundingClientRect()
  return {
    x: (event.clientX - rect.left - offsetX) / scale.value,
    y: (event.clientY - rect.top - offsetY) / scale.value
  }
}
const clamp = (point) => ({ x: Math.max(0, Math.min(imageWidth, point.x)), y: Math.max(0, Math.min(imageHeight, point.y)) })
const cornerIndex = (point) => corners.value.findIndex((item) => Math.hypot(item.x - point.x, item.y - point.y) <= Math.max(12, 10 / scale.value))

const resize = () => {
  if (!canvas.value || !stage.value) return
  canvas.value.width = stage.value.clientWidth
  canvas.value.height = stage.value.clientHeight
  if (imageReady.value && scale.value === 1) {
    scale.value = Math.max(0.1, Math.min(canvas.value.width / imageWidth, canvas.value.height / imageHeight) * 0.92)
    offsetX = (canvas.value.width - imageWidth * scale.value) / 2
    offsetY = (canvas.value.height - imageHeight * scale.value) / 2
  }
  draw()
}

const draw = () => {
  const context = canvas.value?.getContext('2d')
  if (!context) return
  context.clearRect(0, 0, canvas.value.width, canvas.value.height)
  context.fillStyle = '#020617'
  context.fillRect(0, 0, canvas.value.width, canvas.value.height)
  if (!imageReady.value) return
  context.save()
  context.translate(offsetX, offsetY)
  context.scale(scale.value, scale.value)
  context.drawImage(image, 0, 0)
  if (isCorners.value) {
    corners.value.forEach((point, index) => {
      context.strokeStyle = '#ef4444'
      context.lineWidth = 2 / scale.value
      context.beginPath()
      context.arc(point.x, point.y, 8 / scale.value, 0, Math.PI * 2)
      context.stroke()
      context.beginPath()
      context.moveTo(point.x - 6 / scale.value, point.y)
      context.lineTo(point.x + 6 / scale.value, point.y)
      context.moveTo(point.x, point.y - 6 / scale.value)
      context.lineTo(point.x, point.y + 6 / scale.value)
      context.stroke()
      context.fillStyle = '#ef4444'
      context.font = `${12 / scale.value}px sans-serif`
      context.fillText(String(index + 1), point.x + 9 / scale.value, point.y - 9 / scale.value)
    })
  } else if (isLine.value) {
    if (line.value) drawLine(context, line.value, '#22c55e')
    if (drawing && start && current) drawLine(context, { x1: start.x, y1: start.y, x2: current.x, y2: current.y }, '#38bdf8', true)
  } else {
    if (roi.value) drawRect(context, roi.value, '#22c55e')
    if (drawing && start && current) drawRect(context, rectFromPoints(start, current), '#38bdf8')
  }
  context.restore()
}
const rectFromPoints = (a, b) => ({ x: Math.min(a.x, b.x), y: Math.min(a.y, b.y), width: Math.abs(a.x - b.x), height: Math.abs(a.y - b.y) })
const drawRect = (context, rect, color) => {
  context.strokeStyle = color
  context.lineWidth = 2 / scale.value
  context.setLineDash([8 / scale.value, 4 / scale.value])
  context.strokeRect(rect.x, rect.y, rect.width, rect.height)
  context.setLineDash([])
}
const drawLine = (context, value, color, dashed = false) => {
  context.strokeStyle = color
  context.lineWidth = 3 / scale.value
  context.setLineDash(dashed ? [8 / scale.value, 4 / scale.value] : [])
  context.beginPath()
  context.moveTo(value.x1, value.y1)
  context.lineTo(value.x2, value.y2)
  context.stroke()
  context.setLineDash([])
  context.fillStyle = color
  context.beginPath()
  context.arc(value.x1, value.y1, 5 / scale.value, 0, Math.PI * 2)
  context.arc(value.x2, value.y2, 5 / scale.value, 0, Math.PI * 2)
  context.fill()
}

const zoomImage = (event) => {
  if (!imageReady.value) return
  const rect = canvas.value.getBoundingClientRect()
  const x = event.clientX - rect.left
  const y = event.clientY - rect.top
  const imageX = (x - offsetX) / scale.value
  const imageY = (y - offsetY) / scale.value
  scale.value = Math.max(0.08, Math.min(8, scale.value * (event.deltaY < 0 ? 1.12 : 0.88)))
  offsetX = x - imageX * scale.value
  offsetY = y - imageY * scale.value
  draw()
}
const mouseDown = (event) => {
  if (!imageReady.value || event.button !== 0) return
  const point = clamp(imagePoint(event))
  if (isCorners.value) {
    draggingCorner = cornerIndex(point)
    drawing = draggingCorner >= 0
    return
  }
  drawing = true
  start = point
  current = point
  draw()
}
const mouseMove = (event) => {
  if (!imageReady.value) return
  const point = clamp(imagePoint(event))
  if (isCorners.value && draggingCorner >= 0) {
    corners.value[draggingCorner] = point
    draw()
    return
  }
  if (drawing) {
    current = point
    draw()
  }
}
const mouseUp = () => {
  if (isCorners.value) {
    drawing = false
    draggingCorner = -1
    return
  }
  if (!drawing || !start || !current) return
  drawing = false
  if (isLine.value) {
    if (Math.hypot(current.x - start.x, current.y - start.y) >= 5) line.value = { x1: start.x, y1: start.y, x2: current.x, y2: current.y }
  } else {
    const value = rectFromPoints(start, current)
    if (value.width >= 8 && value.height >= 8) roi.value = value
  }
  start = null
  current = null
  draw()
}

const close = () => Window.Close()
const confirm = async () => {
  try {
    if (mode.value === 'pose_calibration') {
      await HIKCalibrationService.AddPoseCalibration(corners.value, rows.value, cols.value, squareSize.value)
    } else if (mode.value === 'calibration_corners') {
      const result = await HIKCalibrationService.AddCalibrationCorners(corners.value, rows.value, cols.value, squareSize.value)
      if (!result?.success) throw new Error(result?.error || '相机标定失败')
      Events.Emit('calibration_added', { data: { count: result.count } })
    } else if (mode.value === 'line') {
      await HIKExtensometerService.SetDirectionLine(line.value)
    } else {
      await HIKExtensometerService.SetROI(label.value, roi.value)
    }
    await close()
  } catch (error) {
    window.alert(error?.message || String(error))
  }
}

const loadImage = (source, initialCorners = []) => {
  corners.value = initialCorners
  image = new Image()
  image.onload = async () => {
    imageWidth = image.naturalWidth
    imageHeight = image.naturalHeight
    imageReady.value = true
    loading.value = false
    await nextTick()
    resize()
  }
  image.src = source
}

onMounted(async () => {
  window.addEventListener('resize', resize)
  try {
    if (isCorners.value) {
      const result = await HIKCalibrationService.FindChessboardCorners(rows.value, cols.value)
      if (!result?.success) throw new Error(result?.error || '棋盘格识别失败')
      loadImage(result.image, result.corners || [])
    } else {
      const frame = await HIKCameraService.GetLatestFrameForROI()
      loadImage(frame.image)
    }
  } catch (error) {
    loading.value = false
    window.alert(error?.message || String(error))
  }
})
onUnmounted(() => window.removeEventListener('resize', resize))
</script>

<style scoped>
.calibration-page { position: fixed; inset: 0; display: flex; flex-direction: column; background: #020617; color: #e2e8f0; font-family: Inter, 'Segoe UI', sans-serif; }
.toolbar, .footer { flex: 0 0 auto; display: flex; align-items: center; justify-content: space-between; padding: 14px 22px; background: #0f172a; border-color: #334155; }
.toolbar { border-bottom: 1px solid #334155; }
.toolbar h1 { margin: 0; font-size: 18px; color: #38bdf8; }
.toolbar p { margin: 5px 0 0; color: #94a3b8; font-size: 12px; }
.zoom, .status { border: 1px solid #334155; background: #111827; border-radius: 7px; padding: 7px 12px; font-size: 12px; }
.stage { position: relative; flex: 1; min-height: 0; overflow: hidden; }
canvas { display: block; width: 100%; height: 100%; cursor: crosshair; }
.message { position: absolute; inset: 0; display: grid; place-items: center; color: #94a3b8; font-weight: 700; }
.message.error { color: #f87171; }
.footer { border-top: 1px solid #334155; gap: 16px; }
.status { display: flex; gap: 12px; color: #94a3b8; }
.status strong { color: #22c55e; }
.real-length { display: flex; align-items: center; gap: 8px; color: #94a3b8; font-size: 12px; }
.real-length input { width: 90px; padding: 6px 8px; border: 1px solid #475569; border-radius: 6px; background: #111827; color: #e2e8f0; font-size: 13px; }
.actions { display: flex; gap: 10px; }
button { border: 1px solid #475569; border-radius: 6px; padding: 9px 28px; min-width: 90px; height: 40px; color: #e2e8f0; background: transparent; font-weight: 700; font-size: 14px; cursor: pointer; box-sizing: border-box; white-space: nowrap; }
button.primary { border-color: #2563eb; background: #2563eb; }
button:disabled { cursor: not-allowed; opacity: .45; }
</style>
