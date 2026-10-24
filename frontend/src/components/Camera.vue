<template>
  <div class="camera-shell">
    <header class="camera-header">
      <div class="header-group">
        <span class="status-badge" :class="{ online: hasFrame }">
          <em></em>
          {{ hasFrame ? '在线' : '无信号' }}
        </span>
        <span class="header-item" title="相机设备">
          <i class="ri-camera-lens-line"></i>
          <b>{{ deviceName || '未连接相机' }}</b>
        </span>
        <span class="header-item" title="图像分辨率">
          <i class="ri-aspect-ratio-line"></i>
          <b>{{ resolutionText }}</b>
        </span>
        <span class="header-item" title="帧率">
          <i class="ri-speed-up-line"></i>
          <b>{{ frameRateText }}</b>
        </span>
      </div>

      <div class="header-actions">
        <button type="button" title="刷新相机参数" @click="loadParams">
          <i class="ri-refresh-line" :class="{ spinning: paramsLoading }"></i>
        </button>
      </div>
    </header>

    <div class="camera-body">
      <section class="viewport">
        <div class="video-stage">
          <img v-show="hasFrame" class="stream-image" :src="frameImage" alt="相机实时画面">

          <div v-if="!hasFrame" class="no-signal">
            <i class="ri-signal-wifi-error-line"></i>
            <strong>NO SIGNAL</strong>
            <span>等待相机图像推送到终端...</span>
          </div>

          <div v-if="hasFrame" class="osd-rec">
            <em></em>
            <span>REC</span>
          </div>
        </div>
      </section>

      <aside class="param-panel">
        <section class="panel-group">
          <h3><i class="ri-equalizer-line"></i> 相机参数</h3>

          <div class="param-item">
            <div class="param-head">
              <label>曝光 (Exposure)</label>
              <select v-model="apply.exposureAuto" @change="applyExposureAuto">
                <option value="manual">手动</option>
                <option value="once_auto">单次自动</option>
                <option value="continuous_auto">连续自动</option>
              </select>
            </div>
            <div class="param-row">
              <input
                v-model.number="cameraParams.exposureTime"
                type="range"
                min="1"
                max="100000"
                step="1"
                @change="applyExposureTime"
              >
              <input
                v-model.number="cameraParams.exposureTime"
                type="number"
                min="1"
                max="100000"
                step="1"
                @keyup.enter="applyExposureTime"
              >
              <em>μs</em>
            </div>
          </div>

          <div class="param-item">
            <div class="param-head">
              <label>增益 (Gain)</label>
              <select v-model="apply.gainAuto" @change="applyGainAuto">
                <option value="manual">手动</option>
                <option value="once_auto">单次自动</option>
                <option value="continuous_auto">连续自动</option>
              </select>
            </div>
            <div class="param-row">
              <input
                v-model.number="cameraParams.gain"
                type="range"
                min="0"
                max="24"
                step="0.1"
                @change="applyGain"
              >
              <input
                v-model.number="cameraParams.gain"
                type="number"
                min="0"
                max="24"
                step="0.1"
                @keyup.enter="applyGain"
              >
              <em>dB</em>
            </div>
          </div>

          <div class="param-item">
            <div class="param-head">
              <label>数字增益 (Digital Gain)</label>
              <input v-model="cameraParams.digitalGainEnable" type="checkbox" @change="applyDigitalGainEnabled">
            </div>
            <div class="param-row">
              <input
                v-model.number="cameraParams.digitalGain"
                type="range"
                min="0"
                max="8"
                step="1"
                :disabled="!cameraParams.digitalGainEnable"
                @change="applyDigitalGain"
              >
              <input
                v-model.number="cameraParams.digitalGain"
                type="number"
                min="0"
                max="8"
                step="1"
                :disabled="!cameraParams.digitalGainEnable"
                @keyup.enter="applyDigitalGain"
              >
              <em>×</em>
            </div>
          </div>

          <div class="param-item">
            <div class="param-head">
              <label>伽马 (Gamma)</label>
              <input v-model="cameraParams.gammaEnable" type="checkbox" @change="applyGammaEnabled">
            </div>
            <div class="param-row">
              <input
                v-model.number="cameraParams.gamma"
                type="range"
                min="0.1"
                max="4"
                step="0.1"
                :disabled="!cameraParams.gammaEnable"
                @change="applyGamma"
              >
              <input
                v-model.number="cameraParams.gamma"
                type="number"
                min="0.1"
                max="4"
                step="0.1"
                :disabled="!cameraParams.gammaEnable"
                @keyup.enter="applyGamma"
              >
              <em>γ</em>
            </div>
          </div>

          <div class="param-item">
            <div class="param-head">
              <label>白平衡 (White Balance)</label>
              <select v-model="apply.balanceWhiteAuto" @change="applyBalanceWhiteAuto">
                <option value="manual">手动</option>
                <option value="once_auto">单次自动</option>
                <option value="continuous_auto">连续自动</option>
              </select>
            </div>
            <div class="param-row">
              <span class="channel red">R</span>
              <input
                v-model.number="whiteBalance.red"
                type="range"
                min="0"
                max="4095"
                step="1"
                @change="applyWhiteBalanceRed"
              >
              <input
                v-model.number="whiteBalance.red"
                type="number"
                min="0"
                max="4095"
                step="1"
                @keyup.enter="applyWhiteBalanceRed"
              >
            </div>
            <div class="param-row">
              <span class="channel green">G</span>
              <input
                v-model.number="whiteBalance.green"
                type="range"
                min="0"
                max="4095"
                step="1"
                @change="applyWhiteBalanceGreen"
              >
              <input
                v-model.number="whiteBalance.green"
                type="number"
                min="0"
                max="4095"
                step="1"
                @keyup.enter="applyWhiteBalanceGreen"
              >
            </div>
            <div class="param-row">
              <span class="channel blue">B</span>
              <input
                v-model.number="whiteBalance.blue"
                type="range"
                min="0"
                max="4095"
                step="1"
                @change="applyWhiteBalanceBlue"
              >
              <input
                v-model.number="whiteBalance.blue"
                type="number"
                min="0"
                max="4095"
                step="1"
                @keyup.enter="applyWhiteBalanceBlue"
              >
            </div>
          </div>
        </section>

        <!-- <section class="panel-group">
          <h3><i class="ri-crop-line"></i> 标记 & 方向</h3>
          <div class="calibration-actions">
            <button class="primary" type="button" @click="() => openROISelector('A')">
              <i class="ri-crop-line"></i>
              <span>ROI A</span>
            </button>
            <button class="primary" type="button" @click="() => openROISelector('B')">
              <i class="ri-crop-2-line"></i>
              <span>ROI B</span>
            </button>
            <button type="button" @click="openDirectionSelector">
              <i class="ri-arrow-left-right-line"></i>
              <span>方向线</span>
            </button>
            <button type="button" @click="openCalibrationSelector">
              <i class="ri-ruler-line"></i>
              <span>比例标定</span>
            </button>
          </div>
        </section> -->

        <section class="panel-group">
          <h3><i class="ri-calibrate-line"></i> 相机标定</h3>
          <div class="param-item">
            <div class="param-head">
              <label>棋盘格行数</label>
              <input v-model.number="calibrationRows" type="number" min="1" step="1" @change="applyCalibrationPattern">
            </div>
          </div>
          <div class="param-item">
            <div class="param-head">
              <label>棋盘格列数</label>
              <input v-model.number="calibrationCols" type="number" min="1" step="1" @change="applyCalibrationPattern">
            </div>
          </div>
          <div class="param-item">
            <div class="param-head">
              <label>方格尺寸 (mm)</label>
              <input v-model.number="calibrationSquareSize" type="number" min="0.1" step="0.1" @change="applyCalibrationPattern">
            </div>
          </div>
          <div class="calibration-actions">
            <button class="primary" type="button" @click="openCameraCalibration">
              <i class="ri-calibrate-line"></i>
              <span>相机标定</span>
            </button>
            <button type="button" @click="clearCameraCalibration">
              <i class="ri-delete-bin-6-line"></i>
              <span>清除</span>
            </button>
            <button class="primary" type="button" @click="openPoseCalibration">
              <i class="ri-focus-3-line"></i>
              <span>位姿标定</span>
            </button>
            <button type="button" @click="clearPoseCalibration">
              <i class="ri-eraser-line"></i>
              <span>清除</span>
            </button>
          </div>
        </section>

        <p v-if="errorMessage" class="error-text">
          <i class="ri-error-warning-line"></i>
          <span>{{ errorMessage }}</span>
        </p>
      </aside>
    </div>
  </div>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref } from 'vue';
import { Events } from '@wailsio/runtime';
import {
  GetCameraParams,
  GetHIKCameraDevices,
  GetLatestFrameForROI,
  IsHIKCameraConnected,
  SetBalanceWhiteAuto,
  SetDigitalGain,
  SetDigitalGainEnabled,
  SetExposureAuto,
  SetExposureTime,
  SetGain,
  SetGainAuto,
  SetGamma,
  SetGammaEnabled,
  SetWhiteBalanceBlue,
  SetWhiteBalanceGreen,
  SetWhiteBalanceRed,
} from '../../bindings/MINIMTS/services/devices/hikcameraservice.js';
import {
  ClearCameraCalibration,
  ClearPoseCalibration,
  OpenCameraCalibration,
  OpenPoseCalibration,
  SetCalibrationPattern,
} from '../../bindings/MINIMTS/services/devices/hikcalibrationservice.js';
import {
  OpenCalibrationSelector,
  OpenDirectionSelector,
  OpenROISelector,
} from '../../bindings/MINIMTS/services/devices/hikextensometerservice.js';

// 后端模式值：0 = 手动，1 = 单次自动，2 = 连续自动
const MODE_VALUES = ['manual', 'once_auto', 'continuous_auto'];

const frameImage = ref('');
const frameWidth = ref(0);
const frameHeight = ref(0);
const deviceName = ref('');
const errorMessage = ref('');
const paramsLoading = ref(false);

const cameraParams = reactive({
  exposureAuto: 0,
  exposureTime: 1000,
  gainAuto: 0,
  gain: 0,
  digitalGain: 0,
  digitalGainEnable: false,
  gammaEnable: false,
  gamma: 1,
  balanceWhiteAuto: 0,
  frameRate: 0,
});

const whiteBalance = reactive({ red: 0, green: 0, blue: 0 });

// 下拉框与后端数值的双向映射
const apply = reactive({
  exposureAuto: 'manual',
  gainAuto: 'manual',
  balanceWhiteAuto: 'manual',
});

const hasFrame = computed(() => frameImage.value !== '');
const resolutionText = computed(() =>
  frameWidth.value > 0 ? `${frameWidth.value} × ${frameHeight.value}` : '--'
);
const frameRateText = computed(() =>
  cameraParams.frameRate > 0 ? `${cameraParams.frameRate.toFixed(1)} fps` : '--'
);

const errorText = (error) => error?.message || String(error || '操作失败');

// 统一执行后端相机操作，失败时显示错误提示
const applyParam = async (action) => {
  errorMessage.value = '';
  try {
    await action();
  } catch (error) {
    errorMessage.value = errorText(error);
  }
};

const loadParams = async () => {
  paramsLoading.value = true;
  errorMessage.value = '';
  try {
    const params = await GetCameraParams();
    if (!params) return;

    cameraParams.exposureAuto = params.exposureAuto ?? 0;
    cameraParams.exposureTime = params.exposureTime ?? cameraParams.exposureTime;
    cameraParams.gainAuto = params.gainAuto ?? 0;
    cameraParams.gain = params.gain ?? cameraParams.gain;
    cameraParams.digitalGain = params.digitalGain ?? 0;
    cameraParams.digitalGainEnable = Boolean(params.digitalGainEnable);
    cameraParams.gammaEnable = Boolean(params.gammaEnable);
    cameraParams.gamma = params.gamma ?? cameraParams.gamma;
    cameraParams.balanceWhiteAuto = params.balanceWhiteAuto ?? 0;
    cameraParams.frameRate = params.frameRate ?? 0;

    apply.exposureAuto = MODE_VALUES[cameraParams.exposureAuto] || 'manual';
    apply.gainAuto = MODE_VALUES[cameraParams.gainAuto] || 'manual';
    apply.balanceWhiteAuto = MODE_VALUES[cameraParams.balanceWhiteAuto] || 'manual';
  } catch (error) {
    errorMessage.value = errorText(error);
  } finally {
    paramsLoading.value = false;
  }
};

const loadDevices = async () => {
  try {
    const devices = (await GetHIKCameraDevices()) || [];
    if (devices.length > 0) {
      deviceName.value = devices[0];
    }
  } catch {
    // 设备枚举失败不影响画面显示，忽略
  }
};

const loadLatestFrame = async () => {
  try {
    const frame = await GetLatestFrameForROI();
    if (frame?.image) {
      frameImage.value = frame.image;
      frameWidth.value = frame.width || 0;
      frameHeight.value = frame.height || 0;
    }
  } catch {
    // 暂无图像时保持 NO SIGNAL 占位
  }
};

const applyExposureAuto = () => applyParam(() => SetExposureAuto(apply.exposureAuto));
const applyExposureTime = () => applyParam(() => SetExposureTime(Number(cameraParams.exposureTime)));
const applyGainAuto = () => applyParam(() => SetGainAuto(apply.gainAuto));
const applyGain = () => applyParam(() => SetGain(Number(cameraParams.gain)));
const applyDigitalGainEnabled = () =>
  applyParam(() => SetDigitalGainEnabled(Boolean(cameraParams.digitalGainEnable)));
const applyDigitalGain = () => applyParam(() => SetDigitalGain(Number(cameraParams.digitalGain)));
const applyGammaEnabled = () =>
  applyParam(() => SetGammaEnabled(Boolean(cameraParams.gammaEnable)));
const applyGamma = () => applyParam(() => SetGamma(Number(cameraParams.gamma)));
const applyBalanceWhiteAuto = () =>
  applyParam(() => SetBalanceWhiteAuto(apply.balanceWhiteAuto));
const applyWhiteBalanceRed = () =>
  applyParam(() => SetWhiteBalanceRed(Number(whiteBalance.red)));
const applyWhiteBalanceGreen = () =>
  applyParam(() => SetWhiteBalanceGreen(Number(whiteBalance.green)));
const applyWhiteBalanceBlue = () =>
  applyParam(() => SetWhiteBalanceBlue(Number(whiteBalance.blue)));

const openCameraCalibration = () => applyParam(OpenCameraCalibration);
const openPoseCalibration = () => applyParam(OpenPoseCalibration);
const clearCameraCalibration = () => applyParam(ClearCameraCalibration);
const clearPoseCalibration = () => applyParam(ClearPoseCalibration);
const openROISelector = (label) => applyParam(() => OpenROISelector(label));
const openDirectionSelector = () => applyParam(OpenDirectionSelector);
const openCalibrationSelector = () => applyParam(OpenCalibrationSelector);

const calibrationRows = ref(7);
const calibrationCols = ref(5);
const calibrationSquareSize = ref(25);
const applyCalibrationPattern = () =>
  applyParam(() => SetCalibrationPattern(calibrationRows.value, calibrationCols.value, calibrationSquareSize.value));

let unsubscribeFrame = null;

onMounted(async () => {
  unsubscribeFrame = Events.On('hik_camera_frame', (event) => {
    if (event?.data?.image) {
      frameImage.value = event.data.image;
    }
  });

  const connected = await IsHIKCameraConnected().catch(() => false);
  if (connected) {
    await Promise.all([loadDevices(), loadLatestFrame(), loadParams()]);
  } else {
    errorMessage.value = '相机未连接，请先在主界面完成设备连接';
  }
});

onBeforeUnmount(() => {
  console.log('Camera component before unmount');
  unsubscribeFrame?.();
});
</script>

<style scoped>
@import 'remixicon/fonts/remixicon.css';

.camera-shell {
  --page-bg: #1e293b;
  --title-bg: #0f172a;
  --panel-bg: #273549;
  --button-bg: #334155;
  --border: #475569;
  --text-main: #f8fafc;
  --text-normal: #cbd5e1;
  --text-muted: #94a3b8;
  --primary: #3b82f6;
  position: fixed;
  inset: 0;
  display: grid;
  grid-template-rows: 46px minmax(0, 1fr);
  min-width: 900px;
  min-height: 560px;
  color: var(--text-normal);
  background: var(--page-bg);
  overflow: hidden;
}

.camera-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 16px;
  padding: 0 14px;
  background: var(--title-bg);
  border-bottom: 1px solid var(--border);
  user-select: none;
}

.header-group {
  min-width: 0;
  display: flex;
  align-items: center;
  gap: 16px;
}

.status-badge {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 4px 10px;
  color: #f87171;
  background: rgba(239, 68, 68, 0.12);
  border: 1px solid rgba(239, 68, 68, 0.4);
  border-radius: 20px;
  font-size: 11px;
  white-space: nowrap;
}

.status-badge em {
  width: 7px;
  height: 7px;
  background: #ef4444;
  border-radius: 50%;
}

.status-badge.online {
  color: #4ade80;
  background: rgba(74, 222, 128, 0.12);
  border-color: rgba(74, 222, 128, 0.4);
}

.status-badge.online em {
  background: #4ade80;
}

.header-item {
  min-width: 0;
  display: flex;
  align-items: center;
  gap: 6px;
  color: var(--text-muted);
  font-size: 12px;
  white-space: nowrap;
}

.header-item i {
  font-size: 15px;
}

.header-item b {
  color: var(--text-normal);
  font-weight: 500;
}

.header-actions button {
  width: 32px;
  height: 32px;
  margin: 0;
  padding: 0;
  display: grid;
  place-items: center;
  color: var(--text-muted);
  background: transparent;
  border: none;
  border-radius: 8px;
  font-size: 17px;
  cursor: pointer;
  transition: color 0.2s ease, background-color 0.2s ease;
}

.header-actions button:hover {
  color: var(--text-main);
  background: rgba(148, 163, 184, 0.14);
}

.spinning {
  animation: spin 0.9s linear infinite;
}

@keyframes spin {
  to {
    transform: rotate(360deg);
  }
}

.camera-body {
  min-height: 0;
  display: grid;
  grid-template-columns: minmax(0, 1fr) 300px;
  gap: 12px;
  padding: 12px;
  overflow: hidden;
}

.viewport {
  min-width: 0;
  min-height: 0;
  display: flex;
}

.video-stage {
  position: relative;
  flex: 1;
  min-width: 0;
  min-height: 0;
  display: grid;
  place-items: center;
  background: #020617;
  border: 1px solid var(--border);
  border-radius: 8px;
  overflow: hidden;
}

.stream-image {
  max-width: 100%;
  max-height: 100%;
  display: block;
  object-fit: contain;
}

.no-signal {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  color: #64748b;
}

.no-signal i {
  font-size: 46px;
}

.no-signal strong {
  color: #94a3b8;
  font-size: 15px;
  letter-spacing: 3px;
}

.no-signal span {
  font-size: 12px;
}

.osd-rec {
  position: absolute;
  top: 10px;
  left: 12px;
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 3px 8px;
  color: #fca5a5;
  background: rgba(2, 6, 23, 0.6);
  border-radius: 4px;
  font-size: 11px;
  letter-spacing: 1px;
}

.osd-rec em {
  width: 7px;
  height: 7px;
  background: #ef4444;
  border-radius: 50%;
  animation: rec-blink 1.2s ease-in-out infinite;
}

@keyframes rec-blink {
  50% {
    opacity: 0.25;
  }
}

.param-panel {
  min-height: 0;
  display: flex;
  flex-direction: column;
  gap: 10px;
  overflow-y: auto;
}

.panel-group {
  padding: 12px;
  background: var(--panel-bg);
  border: 1px solid var(--border);
  border-radius: 8px;
}

h3 {
  display: flex;
  align-items: center;
  gap: 7px;
  margin: 0 0 10px;
  color: var(--text-main);
  font-size: 13px;
  font-weight: 600;
}

h3 i {
  color: var(--primary);
  font-size: 16px;
}

.param-item + .param-item {
  margin-top: 12px;
  padding-top: 12px;
  border-top: 1px solid rgba(71, 85, 105, 0.6);
}

.param-head {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 8px;
  margin-bottom: 7px;
}

.param-head label {
  color: var(--text-normal);
  font-size: 12px;
}

.param-head select {
  height: 26px;
  padding: 0 6px;
  color: var(--text-normal);
  background: var(--title-bg);
  border: 1px solid var(--border);
  border-radius: 5px;
  font-size: 11px;
  outline: none;
  cursor: pointer;
}

.param-head input[type='checkbox'] {
  width: 15px;
  height: 15px;
  accent-color: var(--primary);
  cursor: pointer;
}

.param-row {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) 62px auto;
  align-items: center;
  gap: 7px;
}

.param-row + .param-row {
  margin-top: 6px;
}

.param-row input[type='range'] {
  grid-column: 2;
  width: 100%;
  height: 4px;
  accent-color: var(--primary);
  cursor: pointer;
}

.param-row input[type='number'] {
  grid-column: 3;
  width: 100%;
  height: 26px;
  padding: 0 6px;
  color: var(--text-main);
  background: var(--title-bg);
  border: 1px solid var(--border);
  border-radius: 5px;
  font-size: 11px;
  font-variant-numeric: tabular-nums;
  outline: none;
}

.param-row input[type='number']:focus {
  border-color: var(--primary);
}

.param-row input:disabled {
  opacity: 0.45;
  cursor: not-allowed;
}

.param-row em {
  grid-column: 4;
  color: #64748b;
  font-size: 11px;
  font-style: normal;
}

.channel {
  grid-column: 1;
  width: 16px;
  font-size: 11px;
  font-weight: 600;
}

.channel.red {
  color: #f87171;
}

.channel.green {
  color: #4ade80;
}

.channel.blue {
  color: #60a5fa;
}

.calibration-actions {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px;
}

.calibration-actions button {
  width: 100%;
  height: 36px;
  margin: 0;
  padding: 0 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 6px;
  color: var(--text-normal);
  background: var(--button-bg);
  border: 1px solid var(--border);
  border-radius: 6px;
  font-size: 12px;
  white-space: nowrap;
  cursor: pointer;
  transition: background 160ms ease, border-color 160ms ease, color 160ms ease;
}

.calibration-actions button:hover {
  color: #fff;
  background: #3e4d62;
  border-color: #64748b;
}

.calibration-actions button.primary {
  color: #fff;
  background: var(--primary);
  border-color: var(--primary);
}

.calibration-actions button.primary:hover {
  background: #2563eb;
  border-color: #2563eb;
}

.error-text {
  display: flex;
  align-items: flex-start;
  gap: 7px;
  margin: 0;
  padding: 9px 10px;
  color: #fca5a5;
  background: rgba(239, 68, 68, 0.12);
  border: 1px solid rgba(239, 68, 68, 0.4);
  border-radius: 6px;
  font-size: 11px;
}

.error-text i {
  font-size: 14px;
}

.error-text span {
  min-width: 0;
  overflow-wrap: anywhere;
}
</style>