<template>
  <div class="modal-backdrop" @click.self="emit('cancel')">
    <section class="modal-dialog" role="dialog" aria-modal="true" aria-labelledby="project-dialog-title">
      <header class="dialog-header">
        <div>
          <strong id="project-dialog-title">项目设置</strong>
        </div>
      </header>

      <div class="settings-body scrollable">
      <!-- 1. 基本信息 -->
      <div class="config-section">
        <div class="section-tag">基本信息</div>
        <div class="form-row">
          <div class="form-item flex-1">
            <label>实验人</label>
            <input v-model="form.experimenter" type="text" spellcheck="false" placeholder="请输入姓名" />
          </div>
          <div class="form-item flex-1">
            <label>样品编号</label>
            <input v-model="form.sampleNo" type="text" placeholder="请输入编号" />
          </div>
          <div class="form-item flex-1">
            <label>实验时间</label>
            <input v-model="form.testDate" type="text" readonly class="readonly" />
          </div>
        </div>
      </div>

      <!-- 2. 准静态测试参数 -->
      <div class="config-section">
        <div class="section-tag">准静态测试参数</div>
        <div class="quasi-grid">
          <!-- 形状选择 -->
          <div class="sub-pane border-r">
            <label class="pane-title">1. 样品形状</label>
            <select v-model="form.sampleShape" class="standard-select">
              <option value="dogbone">狗骨头状 (Dog-bone)</option>
              <option value="cylinder">圆柱状 (Cylinder)</option>
            </select>
            <div class="sample-preview">
              <svg v-if="form.sampleShape === 'dogbone'" viewBox="0 0 100 60" class="sample-svg">
                <path d="M10,20 L30,20 C35,20 35,28 40,28 L60,28 C65,28 65,20 70,20 L90,20 L90,40 L70,40 C65,40 65,32 60,32 L40,32 C35,32 35,40 30,40 L10,40 Z" fill="none" stroke="#3b82f6" stroke-width="1.5" />
                <line x1="40" y1="45" x2="60" y2="45" stroke="#64748b" stroke-width="1" stroke-dasharray="2,2" />
                <text x="50" y="55" text-anchor="middle" font-size="8" fill="#64748b">L0 标距段</text>
              </svg>
              <svg v-else viewBox="0 0 100 60" class="sample-svg">
                <ellipse cx="50" cy="15" rx="25" ry="8" fill="none" stroke="#3b82f6" stroke-width="1.5" />
                <path d="M25,15 L25,45 A25,8 0 0,0 75,45 L75,15" fill="none" stroke="#3b82f6" stroke-width="1.5" />
                <ellipse cx="50" cy="45" rx="25" ry="8" fill="none" stroke="#3b82f6" stroke-width="1.5" stroke-dasharray="2,2" />
                <line x1="82" y1="15" x2="82" y2="45" stroke="#64748b" stroke-width="1" stroke-dasharray="2,2" />
                <text x="88" y="32" text-anchor="start" font-size="8" fill="#64748b">L0</text>
              </svg>
            </div>
          </div>

          <!-- 尺寸输入 -->
          <div class="sub-pane border-r">
            <label class="pane-title">2. 尺寸参数 (mm)</label>
            <div class="dim-inputs" v-if="form.sampleShape === 'dogbone'">
              <div class="input-unit-group"><span>宽度 W</span><input v-model.number="form.width" type="number" step="0.01" /></div>
              <div class="input-unit-group"><span>厚度 T</span><input v-model.number="form.thickness" type="number" step="0.01" /></div>
              <div class="input-unit-group highlight"><span>标距 L0</span><input v-model.number="form.sectionLength" type="number" step="0.01" @input="autoCalcSpeed" /></div>
              <div class="area-calc">计算截面积: <span>{{ form.width }} × {{ form.thickness }} = {{ currentArea }} </span> mm²</div>
            </div>
            <div class="dim-inputs" v-else>
              <div class="input-unit-group"><span>直径 D</span><input v-model.number="form.diameter" type="number" step="0.01" /></div>
              <div class="input-unit-group highlight"><span>标距 L0</span><input v-model.number="form.sectionLength" type="number" step="0.01" @input="autoCalcSpeed" /></div>
              <div class="area-calc">计算截面积: <span>π × {{ form.diameter }}² = {{ currentArea }} </span> mm²</div>
            </div>
          </div>

          <!-- 测试控制 -->
          <div class="sub-pane">
            <label class="pane-title">3. 测试控制</label>
            <div class="form-item">
              <label>试验类型</label>
              <div class="toggle-group">
                <button :class="{ active: form.type === 'tension' }" @click="form.type = 'tension'">拉伸 (T)</button>
                <button :class="{ active: form.type === 'compression' }" @click="form.type = 'compression'">压缩 (C)</button>
              </div>
            </div>
            <div class="form-item mt-10">
              <label>运行速度 (mm/s)</label>
              <input v-model.number="form.speed" type="number" step="0.001" :class="{ 'danger-text': isSpeedOverridden }" @input="handleSpeedManualChange" />
            </div>
            <div class="form-item mt-10">
              <label>暂停状态</label>
              <select v-model="form.pauseCondition">
                <option value="stop">立即停止</option>
                <option value="load-maintain">载荷维持</option>
                <option value="disp-maintain">位移维持(暂不使用)</option>
              </select>
            </div>
          </div>
        </div>

        <!-- 保存与导出 -->
        <div class="quasi-footer border-t mt-15 pt-15">
          <div class="form-item flex-2">
            <label>准静态数据保存路径</label>
            <div class="path-picker">
              <input v-model="form.filePath" type="text" readonly />
              <button class="btn-picker" @click="handleSelectDir('filePath')">...</button>
            </div>
          </div>
          <div class="form-item flex-1">
            <label>保存文件名</label>
            <input v-model="form.fileName" type="text" placeholder="Result_001" />
          </div>
        </div>
      </div>

            <!-- 4. 触发信号发射（独立于相机连接） -->
        <div class="config-section">
          <div class="section-tag">触发信号发射</div>
          <div class="trigger-settings">
            <label class="switch-item">
              <input v-model="form.triggerEnabled" type="checkbox" />
              <span class="switch-track"><span class="switch-thumb"></span></span>
              <span>使能触发信号</span>
            </label>
            <div v-if="form.triggerEnabled" class="trigger-fields animate-in">
              <div class="trigger-field">
                <label>触发周期 (ms)</label>
                <input v-model.number="form.triggerPeriodMs" type="number" min="1" step="1" />
              </div>
              <div class="trigger-field">
                <label>触发脉宽 (ms)</label>
                <input
                  v-model.number="form.triggerPulseWidthMs"
                  type="number"
                  min="1"
                  :max="form.triggerPeriodMs"
                  step="1"
                  :class="{ 'danger-text': !isTriggerValid }"
                />
              </div>
              <div v-if="!isTriggerValid" class="validation-message">触发脉宽必须大于 0 且不能超过触发周期</div>
            </div>
          </div>
        </div>

        <!-- 3. DIC 与 视频引伸 并排 -->
        <div v-if="props.isCameraConnected" class="dual-row">
          <!-- 视频引伸计 -->
          <div class="config-section flex-1">
            <div class="section-tag color-video">视频引伸计 (AVE)</div>
            <div class="panel-body">
              <label class="switch-item text-cyan">
                <input type="checkbox" v-model="form.videoExtEnable" />
                <span class="switch-track"><span class="switch-thumb"></span></span>
                <span>变形测算</span>
              </label>
              <div class="right-display">
                <div v-if="!form.videoExtEnable" class="placeholder">
                  <svg viewBox="0 0 100 60" class="diag-svg">
                    <rect x="42" y="10" width="16" height="40" fill="none" stroke="#334155" stroke-width="1.5"/>
                    <circle cx="50" cy="22" r="3" fill="#06b6d4" />
                    <circle cx="50" cy="38" r="3" fill="#06b6d4" />
                    <path d="M60,22 L70,22 M60,38 L70,38 M65,22 L65,38" stroke="#06b6d4" stroke-width="0.5"/>
                  </svg>
                  <span>AVE 测算已关闭</span>
                </div>

                <div v-else class="ave-flow animate-in">
                  <div class="dir-row">
                    <button class="btn-ave" @click="openDirectionSelector"><i class="ri-edit-2-line"></i> 绘制方向</button>
                    <div class="dir-info-line">{{ form.directionLine || '未绘制' }}</div>
                  </div>
                  <div class="dir-row">
                    <button class="btn-ave mini" @click="openMarkerSelector('A')">选取 A 点</button>
                    <div class="res-box">{{ form.markerA || '--, --' }}</div>
                  </div>
                  <div class="dir-row">
                    <button class="btn-ave mini" @click="openMarkerSelector('B')">选取 B 点</button>
                    <div class="res-box">{{ form.markerB || '--, --' }}</div>
                  </div>
                </div>
              </div>
            </div>
          </div>
          <!-- DIC 图像配置 -->
          <div class="config-section flex-1">
            <div class="section-tag color-dic">DIC 采集配置</div>
            <div class="panel-body">
              <label class="switch-item">
                <input type="checkbox" v-model="form.dicEnable" />
                <span class="switch-track"><span class="switch-thumb"></span></span>
                <span>相机拍摄图片</span>
              </label>
              <div class="right-display">
                <div v-if="!form.dicEnable" class="placeholder">
                  <i class="ri-camera-lens-line"></i>
                  <span>启用 DIC 配置</span>
                </div>

                <div v-if="form.dicEnable" class="expand-group animate-in">
                  <div class="form-item">
                    <label>图片保存路径</label>
                    <div class="path-picker">
                      <input v-model="form.dicFolder" type="text" placeholder="选择保存目录" />
                      <button class="btn-picker" @click="handleSelectDir('dicFolder')">...</button>
                    </div>
                  </div>
                  <div class="form-item mt-5">
                    <label>命名前缀</label>
                    <input v-model="form.dicFileName" type="text" />
                  </div>
                  <div class="form-item mt-5">
                    <label>拍摄周期 (ms)</label>
                    <input v-model.number="form.dicCapturePeriodMs" type="number" min="1" step="1" />
                  </div>
                </div>
              </div>
            </div>
          </div>
        </div>


      </div>

      <footer class="settings-footer">
        <button class="btn-primary" :disabled="loading || saving" @click="handleSubmit">
          <i class="ri-checkbox-circle-fill"></i> {{ saving ? '保存中...' : '提交项目' }}
        </button>
        <button class="btn-secondary" @click="emit('cancel')">取消</button>
      </footer>
    </section>
  </div>
</template>

<script setup>
import { reactive, ref, computed, inject, onMounted, onUnmounted } from 'vue';
import { Events, Dialogs } from '@wailsio/runtime';
import * as HIKExtensometerService from '../../../bindings/MINIMTS/services/devices/hikextensometerservice.js';
import { GetFromLocalPath, SaveToLocalPath } from '../../../bindings/MINIMTS/services/config/projectconfig.js';

const props = defineProps({
  isCameraConnected: {
    type: Boolean,
    default: false
  }
})

const emit = defineEmits(['submit', 'cancel']);
const toast = inject('toast', () => {});
const isSpeedOverridden = ref(false);
const loading = ref(false);
const saving = ref(false);
const errorText = (error) => error?.message || String(error || '操作失败');

const form = reactive({
  experimenter: 'pims',
  sampleNo: '001',
  testDate: new Date().toLocaleString(),
  sampleShape: 'dogbone',
  width: 10.0,
  thickness: 2.0,
  diameter: 5.0,
  sectionLength: 25.0,
  type: 'tension',
  speed: 0.025,
  stopCondition: 'manual',
  pauseCondition: 'stop',
  // 触发信号发射
  triggerEnabled: false,
  triggerPeriodMs: 1000,
  triggerPulseWidthMs: 1000,
  filePath: './data/',
  fileName: 'Result_001',
  // DIC
  dicEnable: false,
  dicFolder: '',
  dicFileName: 'IMG_',
  dicCapturePeriodMs: 5000,
  // AVE
  videoExtEnable: false,
  markerA: '',
  markerB: '',
  directionLine: '',
  poissonEnable: false
});

const currentArea = computed(() => {
  if (form.sampleShape === 'dogbone') {
    return (form.width * form.thickness).toFixed(3);
  }
  return (Math.PI * Math.pow(form.diameter / 2, 2)).toFixed(3);
});

const isTriggerValid = computed(() => {
  if (!form.triggerEnabled) return true;
  return form.triggerPeriodMs > 0
    && form.triggerPulseWidthMs > 0
    && form.triggerPulseWidthMs <= form.triggerPeriodMs;
});

const autoCalcSpeed = () => {
  if (!isSpeedOverridden.value) {
    form.speed = parseFloat((form.sectionLength * 0.001).toFixed(4));
  }
};

const handleSpeedManualChange = () => {
  const calculated = parseFloat((form.sectionLength * 0.001).toFixed(4));
  isSpeedOverridden.value = (form.speed !== calculated);
};

const handleSelectDir = async (field) => {
  try {
    const path = await Dialogs.OpenFile({
      CanChooseDirectories: true,
      CanChooseFiles: false,
      Title: '选择保存目录',
    });
    if (path) form[field] = path;
  } catch (err) {
    console.error('Path selection failed', err);
  }
};

let offROISelected
let offDirectionSelected

const formatROI = (roi) => {
  if (!roi) return '--, --'
  return `X:${Math.round(roi.x)} Y:${Math.round(roi.y)} W:${Math.round(roi.width)} H:${Math.round(roi.height)}`
}

const openMarkerSelector = async (label) => {
  try {
    await HIKExtensometerService.OpenROISelector(label)
  } catch (err) {
    console.error('Failed to open ROI selector:', err);
  }
}

const openDirectionSelector = async () => {
  try {
    await HIKExtensometerService.OpenDirectionSelector()
  } catch (err) {
    console.error('Failed to open direction selector:', err);
  }
}

const openCalibration = async () => {
  try {
    await HIKExtensometerService.OpenCalibrationSelector()
  } catch (err) {
    console.error('Failed to open calibration selector:', err);
  }
}

const handleROISelected = (payload) => {
  const data = payload?.data ?? payload
  const value = formatROI(data?.roi)
  if (data?.label === 'A') {
    form.markerA = value
  } else if (data?.label === 'B') {
    form.markerB = value
  }
}

const formatLine = (line) => {
  if (!line) return '未绘制'
  return `A(${Math.round(line.x1)},${Math.round(line.y1)})→B(${Math.round(line.x2)},${Math.round(line.y2)})`
}

const handleDirectionSelected = (payload) => {
  const data = payload?.data ?? payload
  if (data?.line) {
    form.directionLine = formatLine(data.line)
  }
}

const loadConfig = async () => {
  loading.value = true;
  try {
    const config = await GetFromLocalPath('');
    if (!config) return;
    Object.assign(form, {
      ...config,
      type: config.testType === -1 ? 'compression' : 'tension',
    });
    isSpeedOverridden.value = form.speed !== Number((form.sectionLength * 0.001).toFixed(4));
  } catch (error) {
    toast(errorText(error), 'error');
  } finally {
    loading.value = false;
  }
};

const handleSubmit = async () => {
  if (!isTriggerValid.value) {
    toast('触发脉宽必须大于 0 且不能超过触发周期', 'error');
    return;
  }
  saving.value = true;
  try {
    const config = {
      experimenter: form.experimenter,
      sampleNo: form.sampleNo,
      testDate: form.testDate,
      sampleShape: form.sampleShape,
      width: form.width,
      thickness: form.thickness,
      diameter: form.diameter,
      sectionArea: Number(currentArea.value),
      sectionLength: form.sectionLength,
      testType: form.type === 'compression' ? -1 : 1,
      speed: form.speed,
      stopCondition: form.stopCondition,
      pauseCondition: form.pauseCondition,
      triggerEnabled: form.triggerEnabled,
      triggerPeriodMs: Number(form.triggerPeriodMs),
      triggerPulseWidthMs: Number(form.triggerPulseWidthMs),
      filePath: form.filePath,
      fileName: form.fileName,
      dicEnable: form.dicEnable,
      dicFolder: form.dicFolder,
      dicFileName: form.dicFileName,
      dicCapturePeriodMs: Number(form.dicCapturePeriodMs),
      videoExtEnable: form.videoExtEnable,
      poissonEnable: form.poissonEnable,
    };
    await SaveToLocalPath('', config);
    emit('submit', config);
  } catch (error) {
    toast(errorText(error), 'error');
  } finally {
    saving.value = false;
  }
};

onMounted(() => {
  offROISelected = Events.On('hik_roi_selected', handleROISelected)
  offDirectionSelected = Events.On('hik_direction_selected', handleDirectionSelected)
  loadConfig();
});

onUnmounted(() => {
  offROISelected?.()
  offDirectionSelected?.()
})
</script>

<style scoped>
.modal-backdrop {
  position: fixed;
  inset: 0;
  z-index: 1000;
  display: grid;
  place-items: center;
  padding: 16px;
  background: rgba(2, 6, 23, 0.72);
  --wails-draggable: no-drag;
}

.modal-dialog {
  width: min(800px, calc(100vw - 32px));
  max-height: 80vh;
  padding: 0;
  background: #0f172a;
  border: 1px solid #475569;
  border-radius: 8px;
  box-shadow: 0 24px 70px rgba(0, 0, 0, 0.45);
  overflow: hidden;
  display: flex;
  flex-direction: column;
  color: #f1f5f9;
  font-size: 13px;
}

.dialog-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  padding: 18px 18px 0;
  flex-shrink: 0;
}

.dialog-header strong {
  color: #f8fafc;
  font-size: 16px;
}

.dialog-header button {
  width: 34px;
  height: 34px;
  flex: 0 0 auto;
  margin: 0;
  padding: 0;
  display: grid;
  place-items: center;
  color: #94a3b8;
  background: transparent;
  border: none;
  border-radius: 6px;
  font-size: 20px;
  cursor: pointer;
}

.dialog-header button:hover {
  color: #f8fafc;
  background: rgba(148, 163, 184, 0.14);
}

.settings-body {
  flex: 1;
  padding: 15px;
  display: flex;
  flex-direction: column;
  gap: 15px;
  overflow-y: auto;
  min-height: 0;
}

.scrollable { overflow-y: auto; }

.settings-footer {
  display: flex;
  justify-content: flex-end;
  gap: 12px;
  padding: 12px 20px;
  background: #1e293b;
  border-top: 1px solid #334155;
  flex-shrink: 0;
}

.config-section {
  border: 1px solid #334155; border-radius: 8px; padding: 15px;
  background: #1e293b; position: relative;
}

.section-tag {
  position: absolute; top: -10px; left: 15px; background: #3b82f6;
  padding: 1px 10px; font-size: 11px; font-weight: 900; border-radius: 3px; color: white;
}

.quasi-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 0; margin-top: 5px; }
.sub-pane { padding: 0 15px; display: flex; flex-direction: column; }
.pane-title { font-size: 12px; color: #3b82f6; font-weight: 800; margin-bottom: 10px; display: block; }

.sample-preview {
  height: 160px; background: #0f172a; border-radius: 6px; margin-top: 10px;
  display: flex; align-items: center; justify-content: center;
}
.sample-svg { width: 85%; height: 85%; }

.dim-inputs { display: flex; flex-direction: column; gap: 8px; }
.input-unit-group { display: flex; align-items: center; justify-content: space-between; }
.input-unit-group span { font-size: 12px; color: #94a3b8; }
.input-unit-group input { width: 80px; text-align: right; }
.area-calc { margin-top: 10px; font-size: 15px; color: #64748b; text-align: right; }
.area-calc span { color: #10b981; font-weight: bold; font-family: monospace; }

.quasi-footer { display: flex; gap: 15px; }
.flex-2 { flex: 2; }

.toggle-group { display: flex; background: #0f172a; padding: 3px; border-radius: 3px; }
.toggle-group button {
  flex: 1; border: none; background: transparent; color: #64748b;
  padding: 2px; font-size: 12px; cursor: pointer; border-radius: 4px; font-weight: bold;
}
.toggle-group button.active { background: #334155; color: #f1f5f9; }

.path-picker { display: flex; gap: 6px; }
.path-picker input { flex: 1; background: #0f172a; font-size: 12px; padding: 6px 12px; }
.btn-picker {
  background: #334155; border: none; color: #3b82f6; padding: 0 12px;
  border-radius: 5px; cursor: pointer; font-size: 16px;
}
.btn-picker:hover { background: #3b82f6; color: white; }

.trigger-settings { display: flex; align-items: center; gap: 30px; margin-top: 5px; }
.trigger-settings .switch-item { align-self: center; }
.trigger-fields { flex: 1; min-width: 0; display: flex; align-items: center; gap: 30px; flex-wrap: wrap; background: rgba(0,0,0,0.15); border-radius: 8px; padding: 12px; }
.trigger-field { display: flex; align-items: center; gap: 10px; }
.trigger-field label { font-size: 12px; color: #94a3b8; font-weight: 800; white-space: nowrap; }
.trigger-field input { width: 160px; }
.validation-message { color: #ef4444; font-size: 11px; flex-basis: 100%; }

/* switch 开关样式 */
.switch-item { display: inline-flex; align-items: center; gap: 8px; cursor: pointer; font-weight: 800; align-self: flex-start; }
.switch-item input { display: none; }
.switch-track { width: 36px; height: 20px; border-radius: 999px; background: #334155; position: relative; transition: background 0.2s; flex-shrink: 0; }
.switch-thumb { position: absolute; top: 2px; left: 2px; width: 16px; height: 16px; border-radius: 50%; background: #94a3b8; transition: all 0.2s; }
.switch-item input:checked + .switch-track { background: #3b82f6; }
.switch-item input:checked + .switch-track .switch-thumb { transform: translateX(16px); background: #f8fafc; }

.panel-body { display: flex; flex-direction: column; gap: 10px; margin-top: 5px; }
.right-display { min-width: 0; background: rgba(0,0,0,0.15); border-radius: 8px; padding: 12px; display: flex; flex-direction: column; justify-content: center; min-height: 88px; }
.right-display .form-row { flex-wrap: wrap; }
.right-display .form-row .form-item { min-width: 130px; }

.placeholder { text-align: center; color: #334155; display: flex; flex-direction: column; align-items: center; gap: 5px; }
.placeholder i { font-size: 40px; opacity: 0.2; }
.diag-svg { width: 80px; opacity: 0.4; }

.ave-flow { display: flex; flex-direction: column; gap: 10px; }
.dir-row { display: flex; align-items: center; gap: 10px; }
.dir-info-line { flex: 1; min-width: 0; font-size: 11px; color: #06b6d4; background: rgba(6,182,212,0.1); border: 1px solid rgba(6,182,212,0.2); border-radius: 4px; padding: 8px 10px; text-align: center; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; line-height: 1.2; }
.marker-row { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
.marker-row .btn-ave.mini { flex-shrink: 0; }
.res-box { flex: 1 1 90px; min-width: 90px; background: #0f172a; border: 1px solid #334155; border-radius: 4px; padding: 4px 6px; font-size: 10px; text-align: center; color: #3b82f6; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.calib-row { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
.calib-input { width: 80px; padding: 6px 8px; border: 1px solid #334155; border-radius: 4px; background: #0f172a; color: #f1f5f9; font-size: 11px; text-align: center; }
.calib-input::placeholder { color: #64748b; }

.btn-ave {
  background: #334155; border: 1px solid #475569; color: white; padding: 8px 16px;
  border-radius: 6px; font-size: 12px; cursor: pointer; font-weight: bold; min-width: 100px; line-height: 1.2;
}
.btn-ave.mini { padding: 6px 14px; font-size: 11px; line-height: 1.2; min-width: 80px; }

.form-row { display: flex; gap: 15px; }
.form-item { display: flex; flex-direction: column; gap: 4px; }
.form-item label { font-size: 12px; color: #94a3b8; font-weight: 800; }
input, select {
  background: #0f172a; border: 1px solid #334155; border-radius: 6px;
  padding: 8px 12px; color: #f1f5f9; font-size: 13px; outline: none; transition: 0.2s;
}
input:focus, select:focus { border-color: #3b82f6; }
.danger-text { color: #ef4444 !important; border-color: #ef4444 !important; }

.settings-footer { padding: 15px 30px; display: flex; justify-content: flex-start; gap: 15px; background: #1e293b; border-top: 1px solid #334155; }
.btn-secondary {
  background: transparent;
  border: 1px solid #334155;
  color: #64748b;
  padding: 5px 25px;
  border-radius: 6px;
  cursor: pointer;
  width: 150px;
  height: 40px;
}

.btn-primary {
  background: #3b82f6;
  color: white;
  border: none;
  padding: 5px 25px;
  border-radius: 6px;
  font-weight: 900;
  cursor: pointer;
  width: 150px;
  height: 40px;
}

.animate-in { animation: slideUp 0.3s ease-out; }
@keyframes slideUp { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: translateY(0); } }

.border-r { border-right: 1px solid #334155; }
.border-t { border-top: 1px solid #334155; }
.mt-15 { margin-top: 15px; }
.mt-10 { margin-top: 10px; }
.mt-5 { margin-top: 5px; }
.pt-15 { padding-top: 15px; }
.pt-10 { padding-top: 10px; }
.text-cyan { color: #06b6d4 !important; }
.flex-1 { flex: 1; }
.dual-row { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 15px; }
.dual-row > .config-section { min-width: 0; }
.color-dic { background: #8b5cf6; }

@media (max-width: 700px) {
  .dual-row { grid-template-columns: 1fr; }
  .settings-body { padding: 12px; }
  .quasi-grid { grid-template-columns: 1fr; gap: 15px; }
  .sub-pane { padding: 0; }
  .border-r { border-right: none; }
  .right-display { padding: 10px; }
  .dir-row { align-items: stretch; flex-direction: column; }
  .dir-row .btn-ave { width: 100%; }
  .quasi-footer { flex-direction: column; }
}
.color-video { background: #06b6d4; }
</style>
