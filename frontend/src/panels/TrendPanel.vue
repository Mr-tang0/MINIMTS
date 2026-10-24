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

const MAX_POINTS = 50000;
const timeData = [];
const loadData = [];
const motorDispData = [];
const videoPosData = [];
const motorStrainData = [];
const videoStrainData = [];
const stressData = [];

function appendPoint(target, value) {
  target.push(Number.isFinite(Number(value)) ? Number(value) : 0);
  if (target.length > MAX_POINTS) target.shift();
}

function appendHeartbeat(data) {
  if (!data) return;

  appendPoint(timeData, data.time);
  appendPoint(loadData, data.load);
  appendPoint(motorDispData, data.disp);
  appendPoint(videoPosData, data.ePos);
  appendPoint(motorStrainData, data.strain);
  appendPoint(videoStrainData, data.eStrain);
  appendPoint(stressData, data.stress);
  updateChart();
}

function getAxisRange(series) {
  const values = series.filter(Number.isFinite);
  if (values.length === 0) return { min: 0, max: 1 };

  const min = Math.min(...values);
  const max = Math.max(...values);
  if (min !== max) return { min, max };

  const padding = Math.max(Math.abs(min) * 0.001, 0.0001);
  return { min: min - padding, max: max + padding };
}

function formatAxisLabel(value) {
  const rounded = Math.round((Number(value) + Number.EPSILON) * 10000) / 10000;
  return Object.is(rounded, -0) ? '0' : String(rounded);
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
        name: '载荷', type: 'line', smooth: true, symbol: 'none',
        data: timeData.map((t, i) => [t, loadData[i]]),
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
        { name: '电机位移', type: 'line', yAxisIndex: 0, smooth: true, symbol: 'none',
          data: timeData.map((t, i) => [t, motorDispData[i]]),
          lineStyle: { color: '#3b82f6', width: 2 } },
        { name: '视频位置', type: 'line', yAxisIndex: 1, smooth: true, symbol: 'none',
          data: timeData.map((t, i) => [t, videoPosData[i]]),
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
        { name: '电机应变', type: 'line', yAxisIndex: 0, smooth: true, symbol: 'none',
          data: timeData.map((t, i) => [t, motorStrainData[i]]),
          lineStyle: { color: '#8b5cf6', width: 2 } },
        { name: '视频应变', type: 'line', yAxisIndex: 1, smooth: true, symbol: 'none',
          data: timeData.map((t, i) => [t, videoStrainData[i]]),
          lineStyle: { color: '#06b6d4', width: 2 } },
      ],
    };
  }

  /* 第4组：应力-电机应变 + 应力-视频应变（统一横轴=应变） */
  const strainRange = getAxisRange([...motorStrainData, ...videoStrainData]);
  const stressRange = getAxisRange(stressData);
  return {
    grid, tooltip,
    legend: { data: ['应力-电机应变', '应力-视频应变'], top: 8, textStyle: { color: '#94a3b8', fontSize: 11 } },
    xAxis: { type: 'value', name: '应变 (ε)', nameLocation: 'middle', nameGap: 32,
      ...strainRange, ...axisStyle() },
    yAxis: { type: 'value', name: '应力 (MPa)', nameLocation: 'middle', nameGap: 52,
      ...stressRange, splitNumber: 5, ...axisStyle() },
    series: [
      { name: '应力-电机应变', type: 'line', smooth: false, symbol: 'none', clip: true,
        data: motorStrainData.map((s, i) => [s, stressData[i]]),
        lineStyle: { color: '#3b82f6', width: 2 } },
      { name: '应力-视频应变', type: 'line', smooth: false, symbol: 'none', clip: true,
        data: videoStrainData.map((s, i) => [s, stressData[i]]),
        lineStyle: { color: '#f59e0b', width: 2 } },
    ],
  };
}

function updateChart() {
  if (!chart) return;
  chart.setOption(buildOption(), true);
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
    if (Array.isArray(source)) target.splice(0, target.length, ...source);
  }

  updateChart();
}

defineExpose({ setRealtimeData });

onMounted(() => {
  chart = echarts.init(chartRef.value);
  updateChart();
  unsubscribeHeartbeat = Events.On('mts-heartbeat', (event) => appendHeartbeat(event.data));
  ro = new ResizeObserver(() => chart && chart.resize());
  ro.observe(chartRef.value);
});

onBeforeUnmount(() => {
  unsubscribeHeartbeat?.();
  ro?.disconnect();
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
