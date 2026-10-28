<template>
  <div class="modal-backdrop" @click.self="emit('close')">
    <section class="modal-dialog" role="dialog" aria-modal="true" aria-labelledby="system-dialog-title">
      <header class="dialog-header">
        <div>
          <strong id="system-dialog-title">系统设置</strong>
        </div>
        <button type="button" title="关闭" @click="emit('close')">
          <i class="ri-close-line"></i>
        </button>
      </header>
      <div class="system-page">
        <div class="system-body">
      <!-- 电机模块 -->
      <div class="config-section">
        <div class="section-tag">电机模块</div>
        <div class="form-grid">
          <div class="form-item">
            <label>电机 ID</label>
            <input v-model.number="config.motor_id" type="number" min="0" max="255">
          </div>
          <div class="form-item">
            <label>电机分辨率 (pulse/mm)</label>
            <input v-model.number="config.motor_resolution" type="number" step="any">
          </div>
          <div class="form-item">
            <label>电机方向</label>
            <select v-model.number="config.motor_direction">
              <option :value="1">正向 (+1)</option>
              <option :value="-1">反向 (-1)</option>
            </select>
          </div>
        </div>
      </div>

      <!-- 称重模块 -->
      <div class="config-section">
        <div class="section-tag color-weigh">称重模块</div>
        <div class="form-grid">
          <div class="form-item">
            <label>称重模块 ID</label>
            <input v-model.number="config.weigh_id" type="number" min="0" max="255">
          </div>
          <div class="form-item">
            <label>称重分辨率 (unit/N)</label>
            <input v-model.number="config.weigh_resolution" type="number" step="any">
          </div>
          <div class="form-item">
            <label>称重方向</label>
            <select v-model.number="config.weigh_direction">
              <option :value="1">正向 (+1)</option>
              <option :value="-1">反向 (-1)</option>
            </select>
          </div>
        </div>
      </div>

      <!-- 限位模块 -->
      <div class="config-section">
        <div class="section-tag color-limit">限位模块</div>
        <div class="form-grid">
          <div class="form-item">
            <label>限位模块 ID</label>
            <input v-model.number="config.limit_id" type="number" min="0" max="255">
          </div>
          <div class="form-item">
            <label>限位方向</label>
            <select v-model.number="config.limit_direction">
              <option :value="1">端口 1</option>
              <option :value="2">端口 2</option>
            </select>
          </div>
          <div class="form-item checkbox-item">
            <label class="check-item">
              <input v-model="config.limit_enabled" type="checkbox">
              <span class="mark"></span>
              启用限位模块
            </label>
          </div>
        </div>
      </div>

      <!-- 温度模块 -->
      <div class="config-section">
        <div class="section-tag color-temp">温度模块</div>
        <div class="form-grid">
          <div class="form-item">
            <label>温度模块 ID</label>
            <input v-model.number="config.temp_id" type="number" min="0" max="255">
          </div>
          <div class="form-item">
            <label>温度偏移量</label>
            <input v-model.number="config.temp_offset" type="number" step="1">
          </div>
          <div class="form-item checkbox-item">
            <label class="check-item">
              <input v-model="config.temp_enabled" type="checkbox">
              <span class="mark"></span>
              启用温度模块
            </label>
          </div>
        </div>
      </div>

      <!-- PID 控制 -->
      <div class="config-section">
        <div class="section-tag color-pid">恒力 PID 控制</div>
        <div class="form-grid">
          <div class="form-item">
            <label>比例系数 P</label>
            <input v-model.number="config.pid_p" type="number" step="any">
          </div>
          <div class="form-item">
            <label>积分系数 I</label>
            <input v-model.number="config.pid_i" type="number" step="any">
          </div>
          <div class="form-item">
            <label>微分系数 D</label>
            <input v-model.number="config.pid_d" type="number" step="any">
          </div>
          <div class="form-item">
            <label>控制周期 (s)</label>
            <input v-model.number="config.pid_delta_t" type="number" min="0.001" step="0.001">
          </div>
          <div class="form-item">
            <label>最大速度 (mm/s)</label>
            <input v-model.number="config.pid_max_speed" type="number" step="0.001">
          </div>
          <div class="form-item">
            <label>最小速度 (mm/s)</label>
            <input v-model.number="config.pid_min_speed" type="number" step="0.001">
          </div>
          <div class="form-item">
            <label>PID 方向</label>
            <select v-model.number="config.pid_dir">
              <option :value="1">正向 (+1)</option>
              <option :value="-1">反向 (-1)</option>
            </select>
          </div>
        </div>
      </div>

      <!-- DIC 预处理 -->
      <div class="config-section">
        <div class="section-tag color-dic">DIC 预处理</div>
        <div class="form-grid">
          <div class="form-item">
            <label>预处理方法（实时生效）</label>
            <select v-model="config.dic_preprocess_method" @change="applyDICPreprocess">
              <option value="none">无处理</option>
              <option value="gauss">高斯去噪 + 锐化</option>
              <option value="bilateral">双边滤波（保边缘）</option>
              <option value="nlm">非局部均值去噪</option>
              <option value="highpass">光照平场（高通）</option>
            </select>
          </div>
          <div class="form-item" v-if="config.dic_preprocess_method === 'gauss' || config.dic_preprocess_method === 'bilateral'">
            <label>锐化强度</label>
            <input v-model.number="config.dic_sharpen_amount" type="number" step="0.1" min="0" @change="applyDICPreprocess">
          </div>
          <div class="form-item">
            <label>提示</label>
            <span class="hint-text">修改方法后建议重新框选 ROI，使模板与实时帧预处理一致</span>
          </div>
        </div>
      </div>

      <!-- 噪声底测量（调试） -->
      <div class="config-section">
        <div class="section-tag color-noise">噪声底测量（调试）</div>
        <div class="form-grid">
          <div class="form-item">
            <label>采样帧数 (50–2000)</label>
            <input v-model.number="noiseCount" type="number" min="50" max="2000" step="50">
          </div>
          <div class="form-item checkbox-item">
            <button class="btn-primary noise-btn" type="button" :disabled="noiseLoading || loading" @click="measureNoise">
              <i class="ri-pulse-line"></i> {{ noiseLoading ? '测量中...' : '一键测量' }}
            </button>
          </div>
        </div>
        <div v-if="noiseResult" class="noise-result">
          <div class="noise-item">
            <span class="noise-label">有效样本</span>
            <span class="noise-value">{{ noiseResult.count }}</span>
          </div>
          <div class="noise-item">
            <span class="noise-label">距离均值</span>
            <span class="noise-value">{{ noiseResult.mean.toFixed(4) }} px</span>
          </div>
          <div class="noise-item highlight">
            <span class="noise-label">噪声底 (Std)</span>
            <span class="noise-value">{{ noiseResult.std.toFixed(4) }} px</span>
          </div>
          <div class="noise-item highlight" v-if="noiseResult.ratio > 0">
            <span class="noise-label">噪声底 (mm)</span>
            <span class="noise-value">{{ (noiseResult.std / noiseResult.ratio).toFixed(5) }} mm</span>
          </div>
          <div class="noise-item">
            <span class="noise-label">最小值</span>
            <span class="noise-value">{{ noiseResult.min.toFixed(4) }} px</span>
          </div>
          <div class="noise-item">
            <span class="noise-label">最大值</span>
            <span class="noise-value">{{ noiseResult.max.toFixed(4) }} px</span>
          </div>
        </div>
      </div>
        </div>
        <div class="system-footer">
          <button class="btn-secondary" type="button" :disabled="saving" @click="emit('close')">
            取消
          </button>
          <button class="btn-primary" type="button" :disabled="loading || saving" @click="saveConfig">
            <i class="ri-save-3-line"></i> {{ saving ? '保存中...' : '保存设置' }}
          </button>
        </div>
      </div>
    </section>
  </div>
</template>

<script setup>
import { inject, onMounted, reactive, ref } from 'vue';
import {
  GetFromLocalPath,
  SaveToLocalPath,
} from '../../../bindings/MINIMTS/services/config/systemconfig.js';
import {
  GetDICPreprocess,
  MeasureNoiseFloor,
  SetDICPreprocess,
} from '../../../bindings/MINIMTS/services/devices/hikextensometerservice.js';

const emit = defineEmits(['close', 'save']);
const toast = inject('toast', () => {});

// 默认值与后端 NewSystemConfig 保持一致
const defaultConfig = {
  motor_id: 1,
  motor_resolution: 1312.47251,
  motor_direction: 1,
  weigh_id: 2,
  weigh_resolution: 1.0,
  weigh_direction: 1,
  limit_id: 4,
  limit_enabled: true,
  limit_direction: 2,
  temp_id: 5,
  temp_enabled: false,
  temp_offset: 0,
  pid_p: 68,
  pid_i: 0.2,
  pid_d: 5,
  pid_delta_t: 0.2,
  pid_max_speed: 0.3,
  pid_min_speed: -0.3,
  pid_dir: 1,
  dic_preprocess_method: 'gauss',
  dic_sharpen_amount: 0.8,
};

const config = reactive({ ...defaultConfig });
const loading = ref(false);
const saving = ref(false);

// 噪声底测量状态
const noiseCount = ref(300);
const noiseLoading = ref(false);
const noiseResult = ref(null);

const errorText = (error) => error?.message || String(error || '操作失败');

// 实时套用 DIC 预处理到运行中的引伸计
const applyDICPreprocess = async () => {
  try {
    await SetDICPreprocess(config.dic_preprocess_method, Number(config.dic_sharpen_amount) || 0);
    toast('DIC 预处理已实时生效，建议重新框选 ROI 同步模板', 'success');
  } catch (error) {
    toast(errorText(error), 'error');
  }
};

// 一键噪声底测量：静止状态下连续采集 ROI 距离并统计
const measureNoise = async () => {
  if (noiseLoading.value) return;
  noiseLoading.value = true;
  noiseResult.value = null;
  try {
    const result = await MeasureNoiseFloor(Math.round(Number(noiseCount.value) || 300));
    noiseResult.value = result;
    if (result) {
      toast(`测量完成：${result.count} 帧，噪声底 ${result.std.toFixed(4)} px`, 'success');
    }
  } catch (error) {
    toast(errorText(error), 'error');
  } finally {
    noiseLoading.value = false;
  }
};

const loadConfig = async () => {
  loading.value = true;
  try {
    const savedConfig = await GetFromLocalPath('');
    if (savedConfig) {
      Object.assign(config, savedConfig);
    }
    // 以后端运行中的实际预处理设置为准（可能被实时修改过且未保存）
    try {
      const dic = await GetDICPreprocess();
      if (dic && dic.method) {
        config.dic_preprocess_method = dic.method;
        config.dic_sharpen_amount = dic.sharpen ?? config.dic_sharpen_amount;
      }
    } catch { /* 后端不可用时保持配置值 */ }
  } catch (error) {
    toast(errorText(error), 'error');
  } finally {
    loading.value = false;
  }
};

const saveConfig = async () => {
  saving.value = true;
  try {
    await SaveToLocalPath('', { ...config });
    emit('save');
  } catch (error) {
    toast(errorText(error), 'error');
  } finally {
    saving.value = false;
  }
};

onMounted(loadConfig);
</script>

<style scoped>
@import 'remixicon/fonts/remixicon.css';

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
  width: min(720px, calc(100vw - 32px));
  max-height: calc(100vh - 32px);
  padding: 18px;
  background: #0f172a;
  border: 1px solid #475569;
  border-radius: 8px;
  box-shadow: 0 24px 70px rgba(0, 0, 0, 0.45);
  overflow: auto;
}

.dialog-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  margin-bottom: 18px;
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

.system-page {
  display: flex;
  flex-direction: column;
  color: #f1f5f9;
}

.system-body {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.config-section {
  position: relative;
  padding: 20px;
  background: #1e293b;
  border: 1px solid #334155;
  border-radius: 10px;
}

.section-tag {
  position: absolute;
  top: -10px;
  left: 18px;
  padding: 2px 12px;
  background: #3b82f6;
  border-radius: 4px;
  color: white;
  font-size: 11px;
  font-weight: 800;
}

.section-tag.color-weigh { background: #f59e0b; }
.section-tag.color-limit { background: #ef4444; }
.section-tag.color-temp { background: #10b981; }
.section-tag.color-pid { background: #8b5cf6; }
.section-tag.color-dic { background: #06b6d4; }
.section-tag.color-noise { background: #ec4899; }

.form-grid {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 16px;
  margin-top: 5px;
}

.form-item {
  min-width: 0;
  display: flex;
  flex-direction: column;
  gap: 5px;
}

.form-item label {
  color: #94a3b8;
  font-size: 12px;
  font-weight: 700;
}

.form-item.checkbox-item {
  justify-content: center;
}

input,
select {
  width: 100%;
  padding: 8px 12px;
  color: #f1f5f9;
  background: #0f172a;
  border: 1px solid #334155;
  border-radius: 8px;
  outline: none;
  font-size: 13px;
  box-sizing: border-box;
  transition: border-color 0.2s;
}

input:focus,
select:focus {
  border-color: #3b82f6;
}

/* 复选框美化 */
.check-item {
  position: relative;
  display: flex;
  align-items: center;
  gap: 10px;
  color: #f1f5f9;
  font-size: 13px;
  font-weight: 600;
  cursor: pointer;
}

.check-item .mark {
  width: 20px;
  height: 20px;
  flex-shrink: 0;
  position: relative;
  border: 2px solid #334155;
  border-radius: 5px;
  transition: 0.2s;
}

.check-item input[type="checkbox"] {
  display: none;
}

.check-item input[type="checkbox"]:checked + .mark {
  background: #3b82f6;
  border-color: #3b82f6;
}

.check-item input[type="checkbox"]:checked + .mark::after {
  content: '✓';
  position: absolute;
  top: 50%;
  left: 50%;
  transform: translate(-50%, -50%);
  color: white;
  font-size: 12px;
  font-weight: bold;
}

.system-footer {
  display: flex;
  justify-content: flex-end;
  gap: 16px;
  margin: 20px -18px -18px;
  padding: 14px 18px;
  background: #1e293b;
  border-top: 1px solid #334155;
}

.btn-secondary {
  padding: 10px 24px;
  color: #94a3b8;
  background: transparent;
  border: 1px solid #334155;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-secondary:hover:not(:disabled) {
  background: #334155;
  color: #f1f5f9;
}

.btn-primary {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 24px;
  color: white;
  background: #3b82f6;
  border: none;
  border-radius: 8px;
  font-size: 14px;
  font-weight: 700;
  cursor: pointer;
  transition: all 0.2s;
}

.btn-primary:hover:not(:disabled) {
  background: #2563eb;
}

.btn-primary:disabled,
.btn-secondary:disabled {
  cursor: not-allowed;
  opacity: 0.6;
}

/* DIC 预处理提示 */
.hint-text {
  color: #64748b;
  font-size: 12px;
  line-height: 1.7;
}

/* 噪声底测量 */
.noise-btn {
  width: 100%;
  justify-content: center;
}

.noise-result {
  display: grid;
  grid-template-columns: repeat(3, minmax(0, 1fr));
  gap: 12px;
  margin-top: 16px;
  padding: 14px;
  background: #0f172a;
  border: 1px solid #334155;
  border-radius: 8px;
}

.noise-item {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.noise-label {
  color: #64748b;
  font-size: 11px;
  font-weight: 700;
}

.noise-value {
  color: #f1f5f9;
  font-size: 14px;
  font-weight: 700;
  font-variant-numeric: tabular-nums;
}

.noise-item.highlight .noise-value {
  color: #22d3ee;
}
</style>
