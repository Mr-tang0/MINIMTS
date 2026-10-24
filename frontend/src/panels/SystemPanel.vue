<template>
  <div class="settings-grid">
    <section class="settings-section">
      <h4>
        <span class="section-mark blue" aria-hidden="true"></span>
        电机参数
      </h4>
      <label class="form-row">
        <span>设备 ID</span>
        <input v-model.number="config.motor_id" type="number" min="0" max="255">
      </label>
      <label class="form-row">
        <span>电机方向</span>
        <select v-model.number="config.motor_direction">
          <option :value="1">正向</option>
          <option :value="-1">反向</option>
        </select>
      </label>
      <label class="form-row">
        <span>分辨率</span>
        <input v-model.number="config.motor_resolution" type="number" step="any">
      </label>
    </section>

    <section class="settings-section">
      <h4>
        <span class="section-mark orange" aria-hidden="true"></span>
        称重模块
      </h4>
      <label class="form-row">
        <span>设备 ID</span>
        <input v-model.number="config.weigh_id" type="number" min="0" max="255">
      </label>
      <label class="form-row">
        <span>称重方向</span>
        <select v-model.number="config.weigh_direction">
          <option :value="1">正向</option>
          <option :value="-1">反向</option>
        </select>
      </label>
      <label class="form-row">
        <span>分辨率</span>
        <input v-model.number="config.weigh_resolution" type="number" step="any">
      </label>
    </section>

    <section class="settings-section">
      <h4>
        <span class="section-mark red" aria-hidden="true"></span>
        限位模块
      </h4>
      <label class="form-row">
        <span>设备 ID</span>
        <input v-model.number="config.limit_id" type="number" min="0" max="255">
      </label>
      <label class="form-row">
        <span>限位检测</span>
        <select v-model="config.limit_enabled">
          <option :value="true">启用</option>
          <option :value="false">禁用</option>
        </select>
      </label>
      <label class="form-row">
        <span>限位方向</span>
        <select v-model.number="config.limit_direction">
          <option :value="1">端口 1</option>
          <option :value="2">端口 2</option>
        </select>
      </label>
    </section>

    <section class="settings-section">
      <h4>
        <span class="section-mark green" aria-hidden="true"></span>
        温度模块
      </h4>
      <label class="form-row">
        <span>设备 ID</span>
        <input v-model.number="config.temp_id" type="number" min="0" max="255">
      </label>
      <label class="form-row">
        <span>温度检测</span>
        <select v-model="config.temp_enabled">
          <option :value="true">启用</option>
          <option :value="false">禁用</option>
        </select>
      </label>
    </section>

    <div class="actions">
      <button class="cancel-button" type="button" :disabled="saving" @click="emit('close')">
        取消
      </button>
      <button class="save-button" type="button" :disabled="loading || saving" @click="saveConfig">
        {{ saving ? '保存中...' : '保存系统设置' }}
      </button>
    </div>
  </div>
</template>

<script setup>
import { inject, onMounted, reactive, ref } from 'vue';
import {
  GetFromLocalPath,
  SaveToLocalPath,
} from '../../bindings/MINIMTS/services/UI/systemconfig.js';

const emit = defineEmits(['close', 'save']);
const toast = inject('toast', () => {});

const config = reactive({
  motor_id: 0,
  motor_resolution: 0,
  motor_direction: 1,
  weigh_id: 0,
  weigh_resolution: 0,
  weigh_direction: 1,
  limit_id: 0,
  limit_enabled: false,
  limit_direction: 1,
  temp_id: 0,
  temp_enabled: false,
});
const loading = ref(false);
const saving = ref(false);

const errorText = (error) => error?.message || String(error || '操作失败');

const loadConfig = async () => {
  loading.value = true;
  try {
    const savedConfig = await GetFromLocalPath('');
    if (savedConfig) {
      Object.assign(config, savedConfig);
    }
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
.settings-grid {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 12px;
}

.settings-section {
  min-width: 0;
  padding: 13px;
  background: #1e293b;
  border: 1px solid #475569;
  border-radius: 7px;
}

h4 {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 0 0 10px;
  color: #f8fafc;
  font-size: 13px;
}

.section-mark {
  width: 4px;
  height: 16px;
  flex: 0 0 auto;
  border-radius: 2px;
}

.blue { background: #3b82f6; }
.orange { background: #f59e0b; }
.red { background: #ef4444; }
.green { background: #10b981; }

.form-row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 140px;
  align-items: center;
  gap: 8px;
  margin-top: 7px;
  color: #94a3b8;
  font-size: 11px;
}

input,
select {
  width: 100%;
  height: 32px;
  padding: 0 8px;
  color: #e2e8f0;
  background: #0f172a;
  border: 1px solid #475569;
  border-radius: 5px;
  outline: none;
  box-sizing: border-box;
}

input:focus,
select:focus {
  border-color: #3b82f6;
}

.actions {
  grid-column: 1 / -1;
  display: flex;
  justify-content: flex-end;
  gap: 8px;
}

.actions button {
  width: auto;
  min-width: 72px;
  height: 36px;
  margin: 0;
  padding: 0 18px;
  line-height: normal;
  border: 1px solid #475569;
  border-radius: 5px;
  font-size: 12px;
  white-space: nowrap;
  cursor: pointer;
}

.actions button:disabled {
  cursor: not-allowed;
  opacity: 0.55;
}

.cancel-button {
  color: #cbd5e1;
  background: #334155;
}

.save-button {
  color: #fff;
  background: #3b82f6;
  border-color: #3b82f6 !important;
}

@media (max-width: 700px) {
  .settings-grid {
    grid-template-columns: 1fr;
  }

  .actions {
    grid-column: auto;
  }
}
</style>
