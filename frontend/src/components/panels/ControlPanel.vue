<template>
  <aside class="controls">
    <section class="group test-control">
      <h3><i class="ri-dashboard-3-line"></i> 试验控制</h3>
      <div class="test-buttons">
        <button class="start" :class="{ running: measuring }" type="button" @click="toggleMeasurement">
          <i :class="measuring ? 'ri-stop-fill' : 'ri-play-fill'"></i>
          <span>{{ measuring ? '停止试验' : '开始试验' }}</span>
        </button>
        <button class="pause" :class="{ resumed: paused }" type="button" :disabled="!measuring" @click="togglePause">
          <i :class="paused ? 'ri-play-fill' : 'ri-pause-fill'"></i>
          <span>{{ paused ? '继续试验' : '暂停试验' }}</span>
        </button>
      </div>
    </section>

    <section class="group data-control">
      <h3><i class="ri-database-2-line"></i> 数据操作</h3>
      <div class="data-buttons">
        <button type="button" @click="clearData"><i class="ri-delete-bin-6-line"></i><span>清除数据</span></button>
        <button type="button" @click="saveData"><i class="ri-save-3-line"></i><span>保存数据</span></button>
      </div>
    </section>

    <section class="group manual-control">
      <h3><i class="ri-equalizer-2-line"></i> 手动移动</h3>
      <label class="speed-field">
        <span>移动速度</span>
        <input v-model.number="speed" type="number" min="0" step="0.1" :disabled="measuring">
        <em>mm/s</em>
      </label>
      <div class="manual-buttons">
        <button
          class="tensile"
          type="button"
          :disabled="measuring || blockedDirection >= 1"
          @pointerdown.prevent="startJog(1, $event)"
          @pointerup="releaseJog"
          @pointercancel="releaseJog"
        >
          <i class="ri-arrow-up-line"></i><span>拉伸</span>
          <small v-if="blockedDirection >= 1" class="limit-hint">已限位</small>
        </button>
        <button class="stop" type="button" :disabled="measuring" @click="stopMotor">
          <i class="ri-stop-fill"></i><span>停止</span>
        </button>
        <button
          class="compress"
          type="button"
          :disabled="measuring || blockedDirection <= -1"
          @pointerdown.prevent="startJog(-1, $event)"
          @pointerup="releaseJog"
          @pointercancel="releaseJog"
        >
          <i class="ri-arrow-down-line"></i><span>压缩</span>
          <small v-if="blockedDirection <= -1" class="limit-hint">已限位</small>
        </button>
      </div>
    </section>

    <section class="instrument-placeholder" aria-label="仪器图片预留区域">
      <i class="ri-image-add-line"></i>
      <span>仪器图片区域</span>
    </section>
  </aside>
</template>

<script setup>
import { inject, onMounted, onUnmounted, ref } from 'vue';
import { Events } from '@wailsio/runtime';
import {
  ClearDataCache,
  SaveCache,
  MotorJog,
  MotorStop,
  PauseMeasurement,
  ResumeMeasurement,
  StartMeasurement,
  StopMeasurement,
} from '../../../bindings/MINIMTS/services/devices/MINIMTSService.js';

const toast = inject('toast', () => {});

const speed = ref(0.5);
const measuring = ref(false);
const paused = ref(false);
const busy = ref(false);
const jogging = ref(false);
const blockedDirection = ref(0); // 0=无限位；+1=正向挡；-1=反向挡

let offLimitChanged;
onMounted(() => {
  offLimitChanged = Events.On('mts-limit-changed', (event) => {
    const data = event?.data ?? event;
    blockedDirection.value = Number(data?.blockedDirection ?? 0);
    if (blockedDirection.value !== 0) {
      const dir = blockedDirection.value > 0 ? '正向' : '反向';
      toast(`${dir}已被限位，手动${dir === '正向' ? '拉伸' : '压缩'}按钮已禁用`, 'warning');
    } else {
      toast('限位已解除', 'success');
    }
  });
});
onUnmounted(() => {
  offLimitChanged?.();
});

const runAction = async (action) => {
  if (busy.value) return false;

  busy.value = true;
  try {
    await action();
    return true;
  } catch (error) {
    toast(error?.message || String(error || '操作失败'), 'error');
    return false;
  } finally {
    busy.value = false;
  }
};

const toggleMeasurement = async () => {
  if (measuring.value) {
    if (await runAction(StopMeasurement)) {
      measuring.value = false;
      paused.value = false;
      toast('试验已停止', 'success');
    }
    return;
  }
  if (await runAction(StartMeasurement)) {
    measuring.value = true;
    paused.value = false;
    toast('试验已开始', 'success');
  }
};

const togglePause = async () => {
  if (!measuring.value) return;

  if (paused.value) {
    if (await runAction(ResumeMeasurement)) {
      paused.value = false;
      toast('试验已继续', 'success');
    }
    return;
  }
  if (await runAction(PauseMeasurement)) {
    paused.value = true;
    toast('试验已暂停', 'success');
  }
};

const startJog = async (direction, event) => {
  if (measuring.value || jogging.value) return;

  event.currentTarget.setPointerCapture(event.pointerId);
  jogging.value = true;
  const value = Number(speed.value) || 0;
  const started = await runAction(() => MotorJog(direction * value));
  if (!started) jogging.value = false;

  if (!jogging.value) await runAction(MotorStop);
};

const releaseJog = async () => {
  if (!jogging.value) return;

  jogging.value = false;
  if (!busy.value) await runAction(MotorStop);
};

const stopMotor = async () => {
  jogging.value = false;
  await runAction(MotorStop);
};

const saveData = async () => {
  if (await runAction(SaveCache)) toast('数据已保存');
};

const clearData = async () => {
  if (await runAction(ClearDataCache)) {
    Events.Emit('mts-data-cleared');
    toast('数据已清除');
  }
};
</script>

<style scoped>
.controls {
  width: 250px;
  min-height: 0;
  display: flex;
  flex-direction: column;
  gap: 10px;
  flex-shrink: 0;
}

.group {
  padding: 14px;
  background: #273549;
  border: 1px solid #475569;
  border-radius: 8px;
}

h3 {
  display: flex;
  align-items: center;
  gap: 7px;
  margin: 0 0 12px;
  color: #f8fafc;
  font-size: 13px;
  font-weight: 600;
}

h3 i {
  color: #3b82f6;
  font-size: 17px;
}

button {
  width: 100%;
  min-width: 0;
  height: 38px;
  margin: 0;
  padding: 0 12px;
  box-sizing: border-box;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 7px;
  color: #cbd5e1;
  background: #334155;
  border: 1px solid #475569;
  border-radius: 6px;
  font-size: 12px;
  line-height: 1;
  white-space: nowrap;
  cursor: pointer;
  transition: background 160ms ease, border-color 160ms ease, color 160ms ease;
}

button span {
  white-space: nowrap;
}

button:hover:not(:disabled) {
  color: #fff;
  background: #3e4d62;
  border-color: #64748b;
}

button:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}

.test-buttons {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px;
}

.start {
  width: 100%;
  height: 48px;
  color: #fff;
  background: #10b981;
  border-color: #10b981;
  font-size: 16px;
  font-weight: 700;
}

.start:hover {
  background: #059669;
  border-color: #059669;
}

.start i {
  font-size: 19px;
}

.start.running {
  background: #ef4444;
  border-color: #ef4444;
}

.start.running:hover {
  background: #dc2626;
  border-color: #dc2626;
}

.pause {
  height: 48px;
  color: #f8fafc;
  background: #f59e0b;
  border-color: #f59e0b;
  font-size: 14px;
  font-weight: 700;
}

.pause:hover:not(:disabled) {
  background: #d97706;
  border-color: #d97706;
}

.pause.resumed {
  background: #3b82f6;
  border-color: #3b82f6;
}

.pause.resumed:hover {
  background: #2563eb;
  border-color: #2563eb;
}

.pause:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}

.data-buttons {
  display: grid;
  grid-template-columns: repeat(2, minmax(0, 1fr));
  gap: 8px;
}

.data-buttons button {
  height: 54px;
  flex-direction: column;
  gap: 5px;
  padding: 5px 8px;
}

.data-buttons button i {
  color: #60a5fa;
  font-size: 17px;
}

.manual-control {
  display: flex;
  flex-direction: column;
}

.speed-field {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  align-items: center;
  gap: 6px;
  color: #cbd5e1;
  font-size: 12px;
}

.speed-field em {
  color: #64748b;
  font-size: 11px;
  font-style: normal;
}

.speed-field input {
  width: 100%;
  min-width: 0;
  height: 34px;
  padding: 0 8px;
  color: #f8fafc;
  background: #0f172a;
  border: 1px solid #475569;
  border-radius: 5px;
  outline: none;
  font-variant-numeric: tabular-nums;
}

.speed-field input:focus {
  border-color: #3b82f6;
}

.speed-field input:disabled {
  cursor: not-allowed;
  opacity: 0.45;
}

.manual-buttons {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 10px;
}

.manual-buttons button {
  width: 100%;
  height: 40px;
}

.tensile {
  color: #34d399;
}

.stop {
  color: #fbbf24;
}

.compress {
  color: #f87171;
}

.limit-hint {
  margin-left: 4px;
  font-size: 10px;
  opacity: 0.7;
}

.instrument-placeholder {
  min-height: 110px;
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  color: #64748b;
  background: rgba(39, 53, 73, 0.45);
  border: 1px dashed #475569;
  border-radius: 8px;
}

.instrument-placeholder i {
  font-size: 28px;
}

.instrument-placeholder span {
  font-size: 11px;
}
</style>
