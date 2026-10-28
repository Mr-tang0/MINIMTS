<template>
  <section class="chart-panel">
    <header class="panel-header">
      <h2><i class="ri-line-chart-line"></i> 实时数据曲线</h2>
      <div class="tabs">
        <span class="tab-indicator" :style="{ transform: `translateX(${activeTab * 100}%)` }"></span>
        <button v-for="(tab, i) in tabs" :key="i"
          :class="['tab', { active: activeTab === i }]"
          @click="activeTab = i">{{ tab }}</button>
      </div>
    </header>
    <div ref="chartRef" class="chart-wrap"></div>
  </section>
</template>

<script setup>
import { ref, onMounted, onBeforeUnmount, watch } from 'vue';
import * as echarts from 'echarts';
import { Events } from '@wailsio/runtime';

const tabs = ['载荷-时间', '位移-时间', '应变-时间', '应力-应变'];
const activeTab = ref(0);
const chartRef = ref(null);
let chart = null;
let ro = null;
let unsubscribeHeartbeat = null;
let unsubscribeClear = null;
let rafId = null;

const MAX_POINTS = 50000;
const DOWNSAMPLE_THRESHOLD = 10000; // 屏幕最多显示约 1000 个有效点，多余点视觉无意义

// 环形缓冲：避免大数组 shift 的 O(n) 开销
class RingBuffer {
  constructor(max) { this.max = max; this.data = []; this.head = 0; }
  push(v) {
    if (this.data.length < this.max) {
      this.data.push(v);
    } else {
      this.data[this.head] = v;
      this.head = (this.head + 1) % this.max;
    }
  }
  clear() { this.data.length = 0; this.head = 0; }
  replace(source) {
    this.clear();
    if (Array.isArray(source)) {
      for (let i = 0; i < source.length; i++) this.push(source[i]);
    }
  }
  toArray() {
    if (this.data.length < this.max) return this.data;
    const out = new Array(this.max);
    for (let i = 0; i < this.max; i++) out[i] = this.data[(this.head + i) % this.max];
    return out;
  }
  get length() { return this.data.length; }
}

const timeData = new RingBuffer(MAX_POINTS);
const loadData = new RingBuffer(MAX_POINTS);
const motorDispData = new RingBuffer(MAX_POINTS);
const videoPosData = new RingBuffer(MAX_POINTS);
const motorStrainData = new RingBuffer(MAX_POINTS);
const videoStrainData = new RingBuffer(MAX_POINTS);
const stressData = new RingBuffer(MAX_POINTS);

function appendPoint(target, value) {
  target.push(Number.isFinite(Number(value)) ? Number(value) : 0);
}

function appendHeartbeat(data) {
  if (!data) return;
  appendPoint(timeData, data.duration);
  appendPoint(loadData, data.load);
  appendPoint(motorDispData, data.disp);
  appendPoint(videoPosData, data.ePos);
  appendPoint(motorStrainData, data.strain);
  appendPoint(videoStrainData, data.eStrain);
  appendPoint(stressData, data.stress);
  scheduleUpdate();
}

// rAF 节流：避免一帧内多次 setOption
function scheduleUpdate() {
  if (rafId !== null) return;
  rafId = requestAnimationFrame(() => {
    rafId = null;
    updateChart();
  });
}

// for 循环求极值，避免 Math.min(...arr) 展开大数组导致栈溢出
function getAxisRange(buf) {
  const arr = buf.toArray();
  let min = Infinity, max = -Infinity, has = false;
  for (let i = 0; i < arr.length; i++) {
    const v = arr[i];
    if (Number.isFinite(v)) {
      has = true;
      if (v < min) min = v;
      if (v > max) max = v;
    }
  }
  if (!has) return { min: 0, max: 1 };
  if (min === max) {
    const padding = Math.max(Math.abs(min) * 0.001, 0.0001);
    return { min: min - padding, max: max + padding };
  }
  return { min, max };
}

function formatAxisLabel(value) {
  const rounded = Math.round((Number(value) + Number.EPSILON) * 1000) / 1000;
  return Object.is(rounded, -0) ? '0' : String(rounded);
}

// LTTB 降采样：保留趋势与极值，把 N 点降到 threshold 点
function lttbDownsample(points, threshold) {
  const n = points.length;
  if (threshold >= n || threshold <= 0) return points;
  const sampled = [];
  const bucket = (n - 2) / (threshold - 2);
  let a = 0;
  sampled.push(points[0]);
  for (let i = 0; i < threshold - 2; i++) {
    const rangeStart = Math.floor((i + 1) * bucket) + 1;
    const rangeEnd = Math.min(Math.floor((i + 2) * bucket) + 1, n);
    const start = Math.max(rangeStart, 0);
    const end = Math.max(rangeEnd, start + 1);
    const len = end - start;
    let avgX = 0, avgY = 0;
    for (let j = start; j < end; j++) {
      avgX += points[j][0];
      avgY += points[j][1];
    }
    avgX /= len;
    avgY /= len;
    let nextA = start;
    let maxArea = -1;
    const ax = points[a][0], ay = points[a][1];
    for (let j = start; j < end; j++) {
      const area = Math.abs((ax - avgX) * (points[j][1] - ay) - (ax - points[j][0]) * (avgY - ay));
      if (area > maxArea) { maxArea = area; nextA = j; }
    }
    sampled.push(points[nextA]);
    a = nextA;
  }
  sampled.push(points[n - 1]);
  return sampled;
}

function toPoints(xBuf, yBuf) {
  const xs = xBuf.toArray(), ys = yBuf.toArray();
  const len = Math.min(xs.length, ys.length);
  const pts = new Array(len);
  for (let i = 0; i < len; i++) pts[i] = [xs[i], ys[i]];
  return pts;
}

function sample(points) {
  return points.length > DOWNSAMPLE_THRESHOLD
    ? lttbDownsample(points, DOWNSAMPLE_THRESHOLD)
    : points;
}

function dualYAxis(name, position, range, color, showSplitLine) {
  return {
    type: 'value',
    name,
    nameLocation: 'middle',
    nameGap: 52,
    min: range.min,
    max: range.max,
    splitNumber: 5,
    position,
    ...axisStyle(),
    axisLine: { show: true, lineStyle: { color } },
    axisLabel: { color, fontSize: 11, formatter: formatAxisLabel },
    nameTextStyle: { color, fontSize: 11 },
    splitLine: showSplitLine
      ? { lineStyle: { color: 'rgba(71,85,105,0.25)' } }
      : { show: false },
  };
}

/* ---- 图表 ---- */
function axisStyle() {
  return {
    axisLine: { lineStyle: { color: '#475569' } },
    axisLabel: { color: '#94a3b8', fontSize: 11, formatter: formatAxisLabel },
    splitLine: { lineStyle: { color: 'rgba(71,85,105,0.25)' } },
    nameTextStyle: { color: '#94a3b8', fontSize: 11 },
  };
}

function buildOption() {
  const grid = { left: 75, right: 75, top: 50, bottom: 55 };
  const tooltip = {
    trigger: 'axis',
    backgroundColor: 'rgba(15,23,42,0.95)',
    borderWidth: 1,
    borderColor: '#475569',
    textStyle: { color: '#f8fafc', fontSize: 12 },
  };
  const timeRange = getAxisRange(timeData);
  const xBase = {
    type: 'value',
    nameLocation: 'middle',
    nameGap: 32,
    min: timeRange.min,
    max: timeRange.max,
    ...axisStyle(),
  };

  /* 第1组：载荷-时间 */
  if (activeTab.value === 0) {
    return {
      grid, tooltip,
      xAxis: { ...xBase, name: '时间 (s)' },
      yAxis: { type: 'value', name: '载荷 (N)', nameLocation: 'middle', nameGap: 52,
        ...getAxisRange(loadData), ...axisStyle() },
      series: [{
        name: '载荷', type: 'line', symbol: 'none',
        data: sample(toPoints(timeData, loadData)),
        lineStyle: { color: '#3b82f6', width: 2 },
        areaStyle: { color: 'rgba(59,130,246,0.08)' },
      }],
    };
  }

  /* 第2组：电机位移-时间 + 视频位置-时间（独立双纵轴） */
  if (activeTab.value === 1) {
    return {
      grid, tooltip,
      legend: { data: ['电机位移', '视频位置'], top: 8, textStyle: { color: '#94a3b8', fontSize: 11 } },
      xAxis: { ...xBase, name: '时间 (s)' },
      yAxis: [
        dualYAxis('电机位移 (mm)', 'left', getAxisRange(motorDispData), '#3b82f6', true),
        dualYAxis('视频位置 (pix)', 'right', getAxisRange(videoPosData), '#f59e0b', false),
      ],
      series: [
        { name: '电机位移', type: 'line', yAxisIndex: 0, symbol: 'none',
          data: sample(toPoints(timeData, motorDispData)),
          lineStyle: { color: '#3b82f6', width: 2 } },
        { name: '视频位置', type: 'line', yAxisIndex: 1, symbol: 'none',
          data: sample(toPoints(timeData, videoPosData)),
          lineStyle: { color: '#f59e0b', width: 2 } },
      ],
    };
  }

  /* 第3组：电机应变-时间 + 视频应变-时间（独立双纵轴） */
  if (activeTab.value === 2) {
    return {
      grid, tooltip,
      legend: { data: ['电机应变', '视频应变'], top: 8, textStyle: { color: '#94a3b8', fontSize: 11 } },
      xAxis: { ...xBase, name: '时间 (s)' },
      yAxis: [
        dualYAxis('电机应变 (ε)', 'left', getAxisRange(motorStrainData), '#8b5cf6', true),
        dualYAxis('视频应变 (ε)', 'right', getAxisRange(videoStrainData), '#06b6d4', false),
      ],
      series: [
        { name: '电机应变', type: 'line', yAxisIndex: 0, symbol: 'none',
          data: sample(toPoints(timeData, motorStrainData)),
          lineStyle: { color: '#8b5cf6', width: 2 } },
        { name: '视频应变', type: 'line', yAxisIndex: 1, symbol: 'none',
          data: sample(toPoints(timeData, videoStrainData)),
          lineStyle: { color: '#06b6d4', width: 2 } },
      ],
    };
  }

  /* 第4组：应力-电机应变 + 应力-视频应变（统一横轴=应变） */
  const r1 = getAxisRange(motorStrainData);
  const r2 = getAxisRange(videoStrainData);
  const strainRange = { min: Math.min(r1.min, r2.min), max: Math.max(r1.max, r2.max) };
  const stressRange = getAxisRange(stressData);
  return {
    grid, tooltip,
    legend: { data: ['应力-电机应变', '应力-视频应变'], top: 8, textStyle: { color: '#94a3b8', fontSize: 11 } },
    xAxis: { type: 'value', name: '应变 (ε)', nameLocation: 'middle', nameGap: 32,
      ...strainRange, ...axisStyle() },
    yAxis: { type: 'value', name: '应力 (MPa)', nameLocation: 'middle', nameGap: 52,
      ...stressRange, splitNumber: 5, ...axisStyle() },
    series: [
      { name: '应力-电机应变', type: 'line', symbol: 'none', clip: true,
        data: sample(toPoints(motorStrainData, stressData)),
        lineStyle: { color: '#3b82f6', width: 2 } },
      { name: '应力-视频应变', type: 'line', symbol: 'none', clip: true,
        data: sample(toPoints(videoStrainData, stressData)),
        lineStyle: { color: '#f59e0b', width: 2 } },
    ],
  };
}

// 记住图例勾选状态：全量重建时需显式回填
let legendSelected = {};
// 记录当前已渲染的标签页，只有切换标签页才需要全量重建
let renderedTab = -1;

function updateChart() {
  if (!chart) return;
  const option = buildOption();
  if (option.legend) option.legend = { ...option.legend, selected: { ...legendSelected } };

  if (renderedTab !== activeTab.value) {
    renderedTab = activeTab.value;
    // animation: false 关闭曲线更新与坐标轴过渡动画，数据变化时直接重绘
    chart.setOption({ animation: false, ...option }, true);
    return;
  }

  // 实时刷新只增量合并数据与坐标范围，避免整份配置重建导致图例点击被重绘打断
  const pickAxes = (axis) => {
    const list = Array.isArray(axis) ? axis : [axis];
    const patched = list.map((item) => ({ min: item.min, max: item.max }));
    return Array.isArray(axis) ? patched : patched[0];
  };
  chart.setOption({
    animation: false,
    xAxis: pickAxes(option.xAxis),
    yAxis: pickAxes(option.yAxis),
    series: option.series.map((item) => ({ name: item.name, data: item.data })),
  });
}

function clearChartData() {
  for (const buffer of [timeData, loadData, motorDispData, videoPosData, motorStrainData, videoStrainData, stressData]) {
    buffer.clear();
  }
  if (rafId !== null) {
    cancelAnimationFrame(rafId);
    rafId = null;
  }
  updateChart();
}

function setRealtimeData(data) {
  const series = [
    [timeData, data.time],
    [loadData, data.load],
    [motorDispData, data.motorDisp],
    [videoPosData, data.videoPos],
    [motorStrainData, data.motorStrain],
    [videoStrainData, data.videoStrain],
    [stressData, data.stress],
  ];
  for (const [target, source] of series) {
    if (Array.isArray(source)) target.replace(source);
  }
  updateChart();
}

defineExpose({ setRealtimeData });

onMounted(() => {
  chart = echarts.init(chartRef.value);
  chart.on('legendselectchanged', (params) => {
    legendSelected = { ...params.selected };
  });
  updateChart();
  unsubscribeHeartbeat = Events.On('mts-heartbeat', (event) => appendHeartbeat(event.data));
  unsubscribeClear = Events.On('mts-data-cleared', clearChartData);
  ro = new ResizeObserver(() => chart && chart.resize());
  ro.observe(chartRef.value);
});

onBeforeUnmount(() => {
  unsubscribeHeartbeat?.();
  unsubscribeClear?.();
  ro?.disconnect();
  if (rafId !== null) cancelAnimationFrame(rafId);
  chart?.dispose();
});

watch(activeTab, updateChart);
</script>

<style scoped>
.chart-panel {
  min-width: 0;
  min-height: 0;
  display: flex;
  flex-direction: column;
  background: #1e293b;
  border: 1px solid #475569;
  border-radius: 8px;
  overflow: hidden;
}

.panel-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 10px 16px;
  border-bottom: 1px solid rgba(71, 85, 105, 0.5);
  flex-shrink: 0;
}

h2 {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 0;
  color: #f8fafc;
  font-size: 14px;
  white-space: nowrap;
}

h2 i {
  color: #3b82f6;
  font-size: 18px;
}

.tabs {
  position: relative;
  width: 400px;
  height: 34px;
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  padding: 3px;
  box-sizing: border-box;
  overflow: hidden;
  background: #172033;
  border: 1px solid #475569;
  border-radius: 7px;
}

.tab-indicator {
  position: absolute;
  top: 3px;
  bottom: 3px;
  left: 3px;
  width: calc((100% - 6px) / 4);
  background: #3b82f6;
  border-radius: 5px;
  box-shadow: 0 2px 8px rgba(59, 130, 246, 0.3);
  transition: transform 260ms cubic-bezier(0.22, 1, 0.36, 1);
  pointer-events: none;
}

.tab {
  position: relative;
  z-index: 1;
  width: 100%;
  min-width: 0;
  height: 28px;
  margin: 0;
  padding: 0 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #94a3b8;
  background: transparent;
  border: 0;
  border-radius: 5px;
  font-size: 12px;
  line-height: 1;
  white-space: nowrap;
  cursor: pointer;
  transition: color 180ms ease;
}

.tab:hover {
  color: #e2e8f0;
}

.tab.active {
  color: #fff;
  font-weight: 600;
}

.chart-wrap {
  flex: 1;
  min-height: 260px;
  width: 100%;
}
</style>
